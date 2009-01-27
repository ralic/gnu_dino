/****************************************************************************
   Dino - A simple pattern based MIDI sequencer
   
   Copyright (C) 2006  Lars Luthman <lars.luthman@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
****************************************************************************/

#include <iomanip>
#include <iostream>
#include <cassert>
#include <cstring>

#include <jack/midiport.h>
#include <glibmm.h>

#include "debug.hpp"
#include "deleter.hpp"
#include "sequencer.hpp"
#include "song.hpp"
#include "track.hpp"
#include "midibuffer.hpp"
#include "pattern.hpp"


using namespace std;
using namespace sigc;


namespace Dino {

  Sequencer::Sequencer(const string& client_name, Song& song) 
    : m_client_name(client_name), 
      m_song(song), 
      m_valid(false),
      m_cc_resolution(0.01),
      m_time_to_next_cc(0),
      m_last_end(0),
      m_was_rolling(false),
      m_next_beat(0),
      m_next_frame(0),
      m_sent_all_off(false),
      m_current_beat(0), 
      m_old_current_beat(-1),
      m_ports_changed(0),
      m_old_ports_changed(0),
      m_rec(song),
      m_sqbls(0) {
  
    dbg1<<"Initialising sequencer"<<endl;
  
    if (!init_jack(m_client_name)) {
      dbg0<<"Could not initialise JACK!"<<endl;
      dbg0<<"Could not initialise sequencer!"<<endl;
      return;
    }

    m_valid = true;

    m_song.signal_track_added().
      connect(mem_fun(*this, &Sequencer::track_added));
    m_song.signal_track_removed().
      connect(mem_fun(*this, &Sequencer::track_removed));

    Glib::signal_timeout().
      connect(mem_fun(*this, &Sequencer::beat_checker), 20);
    Glib::signal_timeout().
      connect(mem_fun(*this, &Sequencer::ports_checker), 20);
    Glib::signal_timeout().
      connect(bind_return(mem_fun(m_rec, &Recorder::run_editing_thread), true),
              20);
    
    m_rec.signal_track_set.
      connect(mem_fun(*this, &Sequencer::rec_track_changed));

    
    reset_ports();
  }


  Sequencer::~Sequencer() {
    dbg1<<"Destroying sequencer"<<endl;
    if (m_valid) {
      stop();
      go_to_beat(SongTime(0, 0));
      m_valid = false;
      if (m_jack_client)
        jack_client_close(m_jack_client);
    }
  }
  

  void Sequencer::play() {
    if (m_valid)
      jack_transport_start(m_jack_client);
  }


  void Sequencer::stop() {
    if (m_valid)
      jack_transport_stop(m_jack_client);
  }
 

  void Sequencer::go_to_beat(const SongTime& beat) {
    // XXX fix this to take ticks into account
    if (m_valid)
      jack_transport_locate(m_jack_client, m_song.bt2frame(beat.get_beat()));
  }
  
 
  void Sequencer::record_to_track(Song::ConstTrackIterator iter) {
    if (iter == m_song.tracks_end())
      m_rec.set_track(0);
    else
      m_rec.set_track(iter->get_id());
  }
  

  bool Sequencer::is_valid() const {
    return m_valid;
  }


  vector<InstrumentInfo> 
  Sequencer::get_instruments(int track) const {
    vector<InstrumentInfo> instruments;
    if (m_jack_client) {
      
      // check if the given track is connected to a port
      string connected_instrument;
      if (track != -1) {
        map<int,jack_port_t*>::const_iterator iter = m_output_ports.find(track);
        assert(iter != m_output_ports.end());
        const char** connected = jack_port_get_connections(iter->second);
        if (connected && connected[0])
          connected_instrument = connected[0];
        free(connected);
      }
      
      const char** ports = jack_get_ports(m_jack_client, 0, 
                                          JACK_DEFAULT_MIDI_TYPE, 
                                          JackPortIsInput);
      if (ports) {
        for (size_t i = 0; ports[i]; ++i) {
          if (ports[i] == get_jack_name() + ":MIDI input")
            continue;
          InstrumentInfo ii = ports[i];
          if (connected_instrument == ports[i])
            ii.set_connected(true);
          instruments.push_back(ii);
        }
        free(ports);
      }
    }
    return instruments;
  }
 

  void Sequencer::set_instrument(int track, const string& instrument) {
    if (m_jack_client) {
    
      map<int, jack_port_t*>::const_iterator iter = m_output_ports.find(track);
      if (iter == m_output_ports.end()) {
        dbg0<<"Trying to connect nonexistant output port "<<track
            <<" to instrument "<<instrument<<endl;
        return;
      }
      
      // remove old connections from this port
      const char** ports = jack_port_get_connections(iter->second);
      if (ports) {
        for (size_t i = 0; ports[i]; ++i)
          jack_disconnect(m_jack_client, 
                          jack_port_name(iter->second), ports[i]);
        free(ports);
      }
      
      // connect the new instrument
      if (instrument != "None") {
        jack_connect(m_jack_client, 
                     jack_port_name(iter->second), instrument.c_str());
      }
    }
  }


  bool Sequencer::set_instrument(Sequencable& sqb, const string& instrument) {
    return false;
  }
  
  
  void Sequencer::reset_ports() {
    Song::ConstTrackIterator iter;
    for (iter = m_song.tracks_begin(); iter != m_song.tracks_end(); ++iter)
      track_added(iter->get_id());
  }
  
  
  Dino::Song::TrackIterator Sequencer::get_recording_track() {
    int trk;
    if (trk = m_rec.get_track())
      return m_song.tracks_find(trk);
    return m_song.tracks_end();
  }
  

  Sequencable* Sequencer::get_recording_sequencable() {
    return 0;
  }
  
  
  bool Sequencer::init_jack(const string& client_name) {
  
    dbg1<<"Initialising JACK client"<<endl;
    
    jack_set_error_function(&Sequencer::jack_error_function);
    m_jack_client = jack_client_open(client_name.c_str(), JackNullOption, 0);
    if (!m_jack_client)
      return false;
    m_client_name = jack_get_client_name(m_jack_client);
    
    int err;
    dbg1<<"Registering JACK timebase callback"<<endl;
    if ((err = jack_set_timebase_callback(m_jack_client, 0, 
                                          &Sequencer::jack_timebase_callback_,
                                          this)) != 0)
      return false;
    dbg1<<"Registering JACK process callback"<<endl;
    if ((err = jack_set_process_callback(m_jack_client,
                                         &Sequencer::jack_process_callback_,
                                         this)) != 0)
      return false;
    dbg1<<"Registering JACK port registration callback"<<endl;
    if ((err = jack_set_port_registration_callback(m_jack_client,
                                                   &Sequencer::jack_port_registration_callback_,
                                                   this)) != 0)
      return false;
    
    dbg1<<"Registering JACK shutdown callback"<<endl;
    jack_on_shutdown(m_jack_client, &Sequencer::jack_shutdown_handler_, this);
    
    m_input_port = jack_port_register(m_jack_client, "MIDI input", 
                                      JACK_DEFAULT_MIDI_TYPE, 
                                      JackPortIsInput, 0);

    jack_position_t pos;
    memset(&pos, 0, sizeof(pos));
    jack_transport_stop(m_jack_client);
    jack_transport_reposition(m_jack_client, &pos);
    
    if ((err = jack_activate(m_jack_client)) != 0)
      return false;

    return true;
  }
  
  
  void Sequencer::track_added(int track) {
    if (m_valid) {
      char track_name[10];
      sprintf(track_name, "Track %d", track);
      jack_port_t* port = jack_port_register(m_jack_client, track_name, 
                                             JACK_DEFAULT_MIDI_TYPE, 
                                             JackPortIsOutput, 0);
      m_output_ports[track] = port;
    }
  }
  
  
  void Sequencer::track_removed(int track) {
    if (m_valid) {
      jack_port_unregister(m_jack_client, m_output_ports[track]);
      m_output_ports.erase(track);
    }
  }
  
  
  bool Sequencer::add_sequencable(Sequencable& sqb) {
    SeqList* sl = m_sqbls;
    while (sl) {
      if (sl->m_sqb == &sqb)
        return false;
      if (sl->m_next == 0) {
        SeqList* nsl = new SeqList(sqb, m_jack_client);
        nsl->m_prev = sl;
        sl->m_next = nsl;
        return true;
      }
      sl = sl->m_next;
    }
    
    dbg0<<"Sequencable list is broken"<<endl;
    return false;
  }
  
  
  bool Sequencer::remove_sequencable(Sequencable& sqb) {
    SeqList* sl = m_sqbls;
    while (sl) {
      if (sl->m_sqb == &sqb) {
        if (sl->m_next)
          sl->m_next->m_prev = sl->m_prev;
        if (sl->m_prev)
          sl->m_prev->m_next = sl->m_next;
        Deleter::queue(sl);
      }
      sl = sl->m_next;
    }
    return false;
  }
  
  
  void Sequencer::jack_timebase_callback(jack_transport_state_t state, 
                                         jack_nframes_t nframes, 
                                         jack_position_t* pos, int new_pos) {
    int loop_end = m_song.get_loop_end().get_beat();
    int loop_start = m_song.get_loop_start().get_beat();
    bool looping = (loop_end >= 0 && loop_start >= 0 && 
                    loop_end != loop_start);
    
    // these are always the same in Dino
    pos->beats_per_bar = 4;
    pos->ticks_per_beat = 10000;
    
    double beat, bpm;
    
    // if we are standing still or if we just relocated, calculate 
    // the new position
    if (new_pos || state != JackTransportRolling)
      m_song.get_timebase_info(pos->frame, pos->frame_rate, bpm, beat);
    // otherwise, use the "next beat" calculated last time
    else {
      beat = m_next_beat;
      bpm = m_song.get_current_tempo(beat);
    }
    m_next_beat = beat + bpm * double(nframes) / (60 * pos->frame_rate);

    // if we are looping we may need to adjust the BPM so the loop boundary
    // coincides with a JACK period boundary
    if (looping && beat < loop_end) {
      double beats_left = loop_end - beat;
      double periods_left = 60 * (beats_left / bpm) * 
        pos->frame_rate / double(nframes);
      int whole_periods = int(floor(periods_left + 0.5));
      whole_periods = whole_periods == 0 ? 1 : whole_periods;
      bpm = 60 * (beats_left / whole_periods) * 
        pos->frame_rate / double(nframes);
      
      // if this is the last period before the loop end, skip back to the loop
      // start
      if (whole_periods <= 1) {
        jack_transport_locate(m_jack_client, m_song.bt2frame(loop_start));
        m_next_beat = loop_start;
      }
      // otherwise, keep rolling
      else
        m_next_beat = beat + bpm * double(nframes) / (60 * pos->frame_rate);
    }
    
    // fill in the JACK position structure
    pos->beat = int32_t(beat);
    pos->tick = int32_t((beat - pos->beat) * pos->ticks_per_beat);
    pos->bbt_offset = 
      jack_nframes_t((beat - pos->beat - pos->tick / pos->ticks_per_beat) * 
                     pos->frame_rate * 60 / bpm);
    pos->bar = int32_t(pos->beat / pos->beats_per_bar);
    pos->beat %= int(pos->beats_per_bar);
    pos->beats_per_minute = bpm;
    pos->valid = jack_position_bits_t(JackPositionBBT | JackBBTFrameOffset);
    
    // bars and beats start from 1 by convention (but ticks don't!)
    ++pos->bar;
    ++pos->beat;
    
  }
  
  
  int Sequencer::jack_process_callback(jack_nframes_t nframes) {
    jack_position_t pos;
    jack_transport_state_t state = jack_transport_query(m_jack_client, &pos);
    --pos.bar;
    --pos.beat;
    
    // first, tell the GUI thread that it's OK to delete unused objects
    Deleter::get_instance().confirm();
    
    // no valid time info, don't do anything
    if (!(pos.valid & JackTransportBBT))
      return 0;
    
    // set the current beat
    m_current_beat = pos.bar * int(pos.beats_per_bar) + pos.beat;
    
    // at the end of the song, stop and go back to the beginning
    if (m_current_beat >= m_song.get_length().get_beat()) {
      jack_transport_stop(m_jack_client);
      jack_transport_locate(m_jack_client, 0);
      return 0;
    }
    
    sequence_midi(state, pos, nframes);
    void* input_buf = jack_port_get_buffer(m_input_port, nframes);
    m_rec.run_audio_thread(state, pos, nframes, input_buf,
                           jack_get_sample_rate(m_jack_client));
    
    return 0;
  }
  
  
  void Sequencer::jack_shutdown_handler() {
    // XXX do something useful here
    dbg0<<"JACK has shut down!"<<endl;
  }
  
  
  void Sequencer::jack_port_registration_callback(jack_port_id_t port, int m) {
    if (m == 0)
      m_ports_changed = m_ports_changed + 1;
    else {
      int flags = jack_port_flags(jack_port_by_id(m_jack_client, port));
      const char* type = jack_port_type(jack_port_by_id(m_jack_client, port));
      if ((flags & JackPortIsInput) && !strcmp(type, JACK_DEFAULT_MIDI_TYPE))
        m_ports_changed = m_ports_changed + 1;
    }
  }
  
  
  void Sequencer::sequence_midi(jack_transport_state_t state, 
                                const jack_position_t& pos, 
                                jack_nframes_t nframes) {
    // compute the start beat
    double offset;
    if (pos.valid & JackBBTFrameOffset)
      offset = pos.bbt_offset * pos.beats_per_minute / (pos.frame_rate * 60);
    else
      offset = 0;
    double start = pos.bar * pos.beats_per_bar + pos.beat + 
      pos.tick / double(pos.ticks_per_beat) + offset;
    
    // if we're not rolling, turn off all notes and return
    Song::ConstTrackIterator iter;
    if (state != JackTransportRolling) {
      m_was_rolling = false;
      for (iter = m_song.tracks_begin(); iter != m_song.tracks_end(); ++iter) {
        jack_port_t* port = m_output_ports[iter->get_id()];
        if (port) {
          void* port_buf = jack_port_get_buffer(port, nframes);
          jack_midi_clear_buffer(port_buf);
          unsigned char all_notes_off[] = { 0xB0, 123, 0 };
          if (!m_sent_all_off)
            jack_midi_event_write(port_buf, 0, all_notes_off, 3);
        }
        m_sent_all_off = true;
      }
      m_last_end = start;
      return;
    }
    m_sent_all_off = false;
    
    // if we are rolling, sequence MIDI
    // XXX this is very ugly! need some safe way to check if we have relocated
    if (m_was_rolling && pos.frame == m_next_frame)
      start = m_last_end;
    double end = start + pos.beats_per_minute * nframes / 
      (60 * pos.frame_rate);
    m_was_rolling = true;
    m_last_end = end;
    m_next_frame = pos.frame + nframes;
    
    // XXX this is bad - if we are not the timebase master we are not looping,
    // and then we shouldn't change the end
    double loop_end = m_song.get_loop_end().get_beat();
    if (start < loop_end && end > loop_end)
      end = loop_end;
    
    for (iter = m_song.tracks_begin(); iter != m_song.tracks_end(); ++iter) {
      /*
      // get the MIDI buffer
      jack_port_t* port = m_output_ports[iter->get_id()];
      if (port) {
        void* port_buf = jack_port_get_buffer(port, nframes);
        jack_midi_clear_buffer(port_buf);
        MIDIBuffer buffer(port_buf, start, 
                          pos.beats_per_minute, pos.frame_rate);
        buffer.set_period_size(nframes);
        buffer.set_cc_resolution(m_cc_resolution * pos.beats_per_minute / 60);
        iter->sequence(buffer, start, end, m_song.get_length(), -1);
      }
      */
    }
    
    // record MIDI
    void* input_buf = jack_port_get_buffer(m_input_port, nframes);
    jack_midi_event_t input_event;
    jack_nframes_t input_event_index = 0;
    jack_nframes_t input_event_count = jack_midi_get_event_count(input_buf);
    jack_nframes_t timestamp;
    double bpf = pos.beats_per_minute / (60 * pos.frame_rate);
    for (unsigned int i = 0; i < input_event_count; ++i) {
      jack_midi_event_get(&input_event, input_buf, i);
      double beat = start + input_event.time * bpf;
      //m_rec.record_event(beat, input_event.size, input_event.buffer);
    }

  }


  bool Sequencer::beat_checker() {
    int current_beat = m_current_beat;
    if (current_beat != m_old_current_beat) {
      m_old_current_beat = current_beat;
      m_signal_beat_changed(const_cast<int&>(m_current_beat));
    }
    return true;
  }
  
  
  bool Sequencer::ports_checker() {
    int ports_changed = m_ports_changed;
    if (ports_changed != m_old_ports_changed) {
      m_old_ports_changed = ports_changed;
      m_signal_instruments_changed();
    }
    return true;
  }
  
  
  sigc::signal<void, int>& Sequencer::signal_beat_changed() { 
    return m_signal_beat_changed; 
  }
  
  
  sigc::signal<void>& Sequencer::signal_instruments_changed() {
    return m_signal_instruments_changed;
  }


  sigc::signal<void, Song::TrackIterator>& Sequencer::signal_record_to_track(){
    return m_signal_record_to_track;
  }
  

  void Sequencer::rec_track_changed(int id) {
    m_signal_record_to_track(m_song.tracks_find(id));
  }
  

  Sequencer::SeqList::SeqList(Sequencable& sqb, jack_client_t* client)
    : m_sqb(&sqb),
      m_client(client),
      m_port(0),
      m_prev(0),
      m_next(0) {
    m_port = jack_port_register(m_client, m_sqb->get_label().c_str(),
                                JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  }
  
  
  Sequencer::SeqList::~SeqList() {
    if (m_port)
      jack_port_unregister(m_client, m_port);
  }


  
  void Sequencer::print_sequencables() const {
    SeqList* sl = m_sqbls;
    cerr<<endl<<endl;
    while (sl) {
      cerr<<sl->m_sqb->get_label()<<endl;
      sl = sl->m_next;
    }
    cerr<<endl<<endl;
  }

  
  const string& Sequencer::get_jack_name() const {
    return m_client_name;
  }
  
  
}



