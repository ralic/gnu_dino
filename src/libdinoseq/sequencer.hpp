/****************************************************************************
   Dino - A simple pattern based MIDI sequencer
   
   Copyright (C) 2006-2007  Lars Luthman <lars.luthman@gmail.com>
   
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

#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include <map>
#include <string>
#include <vector>

#include <sigc++/signal.h>
#include <jack/jack.h>

#include "debug.hpp"
#include "instrumentinfo.hpp"
#include "recorder.hpp"
#include "ringbuffer.hpp"
#include "song.hpp"


namespace Dino {


  class Song;


  /** This class sequences a Song to a string of MIDI events that are
      sent to JACK MIDI ports. It has functions for starting and stopping
      playback, changing the song position, and also for checking which
      instruments are available and for connecting a Track in the Song
      to one of those instruments. */
  class Sequencer : public sigc::trackable {
  public:
  
    /** This will create a new Sequencer object with the JACK client name 
	@c client_name and the Song object @c song. */
    Sequencer(const std::string& client_name, Song& song);
    ~Sequencer();
    
    /// @name Transport
    //@{
    /** This starts the MIDI playback. */
    void play();
    /** This stops the MIDI playback. */
    void stop();
    /** This seeks to the given beat in the song. */
    void go_to_beat(const SongTime& beat);
    /** This starts recording the given track. */
    void record_to_track(Song::ConstTrackIterator iter);
    //@}
    
    /** Returns @c true if this Sequencer object is valid. */
    bool is_valid() const;
    
    /// @name Instrument control
    //@{
    /** This returns a vector of all instruments that are available for
        the sequencer to play. */
    std::vector<InstrumentInfo> get_instruments(int track = -1) const;
    /** This assigns the given instrument to the given track, i.e. connects
        the track's output port to the instruments input port. This is not
        saved in the .dino file since all connections are supposed to be
        restored by LASH. */
    void set_instrument(int track, const std::string& instrument);
    bool set_instrument(Sequencable& sqb, const std::string& instrument);

    /** This creates new MIDI output ports for all tracks. */
    void reset_ports();
    /** Returns an iterator to the track that is currently being recorded to.*/
    Dino::Song::TrackIterator get_recording_track();
    Sequencable* get_recording_sequencable();
    
    /** Returns the JACK client name. */
    const std::string& get_jack_name() const;
    
    //@}
    
    /** Add a new sequencable object to the sequencer. */
    bool add_sequencable(Sequencable& sqb);
    
    /** Remove a sequencable object from the sequencer. */
    bool remove_sequencable(Sequencable& sqb);
    
    /// @name Signals
    //@{
    sigc::signal<void, int>& signal_beat_changed();
    sigc::signal<void>& signal_instruments_changed();
    sigc::signal<void, Dino::Song::TrackIterator>& signal_record_to_track();
    //@}
    
  private:
    
    class SeqList {
    public:
      SeqList(Sequencable& sqb, jack_client_t* client);
      ~SeqList();
      
      //protected:
      Sequencable* m_sqb;
      jack_client_t* m_client;
      jack_port_t* m_port;
	      
      SeqList* m_prev;
      SeqList* m_next;
    };
    
    
    void print_sequencables() const;
    
    bool beat_checker();
    bool ports_checker();
    
    /** This initialises the internal JACK client object. */
    bool init_jack(const std::string& client_name);
    
    /// @name JACK callbacks
    //@{
    /** This function is called whenever the JACK daemon wants to know the
	beat and tick for a given frame position. */
    void jack_timebase_callback(jack_transport_state_t state, 
				jack_nframes_t nframes, jack_position_t* pos, 
				int new_pos);
    /** This is called once for each JACK cycle. MIDI is sequenced from here. 
	@callgraph
    */
    int jack_process_callback(jack_nframes_t nframes);
    /** This is called when the JACK daemon is being shut down. */
    void jack_shutdown_handler();
    /** This is called when ports are registered or unregistered. */
    void jack_port_registration_callback(jack_port_id_t port, int m);
    //@}
    
    /// @name JACK callback wrappers
    //@{
    static void jack_timebase_callback_(jack_transport_state_t state,
					jack_nframes_t nframes,
					jack_position_t* pos, int new_pos,
					void* arg) {
      static_cast<Sequencer*>(arg)->jack_timebase_callback(state, nframes, 
							   pos, new_pos);
    }
    static int jack_process_callback_(jack_nframes_t nframes, void* arg) {
      return static_cast<Sequencer*>(arg)->jack_process_callback(nframes);
    }
    static void jack_shutdown_handler_(void* arg) {
      static_cast<Sequencer*>(arg)->jack_shutdown_handler();
    }
    static void jack_port_registration_callback_(jack_port_id_t port, int m,
						 void* arg) {
      static_cast<Sequencer*>(arg)->jack_port_registration_callback(port, m);
    }
    static void jack_error_function(const char* msg) {
      Dino::dbg(0, cc+ "JACK: " + msg);
    }
    //@}
    
    /** This function should be called when a new track has been added to the
	song. */
    void track_added(int track);
    /** This function should be called when a track has been removed from the
	song. */
    void track_removed(int track);
  
    /** This function does the actual MIDI playback (i.e. it puts the MIDI
	events on the JACK MIDI output buffers). */
    void sequence_midi(jack_transport_state_t state,
                       const jack_position_t& pos, jack_nframes_t nframes);
    
    void rec_track_changed(int id);
    
    
    std::string m_client_name;
    /* XXX The below is false, I think. Everything should be readable by
       the audio thread at all times without locking any mutii. */
    /** No one is allowed to read or write anything in this variable without
        locking m_song.get_big_mutex() - the exception is the list of tempo 
        changes, which always must be readable for the JACK timebase 
        callback. */
    Song& m_song;
    /** This is @c true if JACK and ALSA has been initialised succesfully. */
    bool m_valid;
  
    jack_client_t* m_jack_client;
    std::map<int, jack_port_t*> m_output_ports;
    jack_port_t* m_input_port;
    
    double m_cc_resolution;
    double m_time_to_next_cc;
    double m_last_end;
    bool m_was_rolling;
    double m_next_beat;
    unsigned long m_next_frame;
    
    bool m_sent_all_off;
    volatile int m_current_beat;
    volatile int m_old_current_beat;
    volatile int m_ports_changed;
    int m_old_ports_changed;

    sigc::signal<void, int> m_signal_beat_changed;
    sigc::signal<void> m_signal_instruments_changed;
    sigc::signal<void, Dino::Song::TrackIterator> m_signal_record_to_track;
    
    struct MIDIEvent {
      double beat;
      unsigned char data[3];
    };
    

    Recorder m_rec;
    
    SeqList* m_sqbls;
    
  };

}
#endif
