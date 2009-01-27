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

#ifndef SONG_HPP
#define SONG_HPP

#include <iterator>
#include <map>
#include <string>

#include <sigc++/signal.h>

#include "songtime.hpp"
#include "tempomap.hpp"


/** This namespace contains front-end independent code for modifying
    and sequencing songs, as well as some utility classes and functions. */
namespace Dino {

  
  class Track;
  

  /** This class contains information about tracks and tempo changes in 
      a song, and some metadata like title, author, and comments. */
  class Song {
  public:
    
    class ConstTrackIterator;
    
    /** An iterator class that is used to access the tracks in the song. */
    class TrackIterator : 
      public std::iterator<std::forward_iterator_tag, Track> {
    public:      
      TrackIterator();
      const Track& operator*() const;
      Track& operator*();
      const Track* operator->() const;
      Track* operator->();
      bool operator==(const TrackIterator& iter) const;
      bool operator!=(const TrackIterator& iter) const;      
      TrackIterator& operator++();
      TrackIterator operator++(int);
    private:
      
      friend class Song;
      friend class ConstTrackIterator;
      
      explicit TrackIterator(const std::map<int, Track*>::iterator& iter);
      
      std::map<int, Track*>::iterator m_iterator;
    };


    /** An iterator class that is used to access the tracks in the song, 
  without changing them. */
    class ConstTrackIterator : 
      public std::iterator<std::forward_iterator_tag, Track> {
    public:
      ConstTrackIterator();
      ConstTrackIterator(const TrackIterator& iter);
      const Track& operator*() const;
      const Track* operator->() const;
      bool operator==(const ConstTrackIterator& iter) const;
      bool operator!=(const ConstTrackIterator& iter) const;
      ConstTrackIterator& operator++();
      ConstTrackIterator operator++(int);
      
    private:
      
      friend class Song;

      explicit ConstTrackIterator(const std::map<int, Track*>::const_iterator&
          iter);
      
      std::map<int, Track*>::const_iterator m_iterator;
    };
    
    
    /** An iterator class that is used to access the tempo changes in the song.
  It is a read-only iterator, you have to use the member functions in 
  Song to modify the tempo map.
  @see Song::add_tempo_change(), Song::remove_tempo_change()
    */
    class TempoIterator : 
      public std::iterator<std::forward_iterator_tag, TempoMap::TempoChange> {
    public:

      TempoIterator();
      TempoMap::TempoChange& operator*();
      TempoMap::TempoChange* operator->();
      bool operator==(const TempoIterator& iter) const;
      bool operator!=(const TempoIterator& iter) const;
      TempoIterator& operator++();
      TempoIterator operator++(int);
      
    private:
      
      TempoIterator(TempoMap::TempoChange* tempo);
      
      friend class Song;

      TempoMap::TempoChange* m_tempo;
    };
    
    
    Song();
    ~Song();
  
    /// @name Accessors
    //@{
    const std::string& get_title() const;
    const std::string& get_author() const;
    const std::string& get_info() const;
    ConstTrackIterator tracks_begin() const;
    ConstTrackIterator tracks_end() const;
    ConstTrackIterator tracks_find(int id) const;
    TempoIterator tempo_begin() const;
    TempoIterator tempo_end() const;
    TempoIterator tempo_find(int beat) const;
    const size_t get_number_of_tracks() const;
    const SongTime& get_length() const;
    const SongTime& get_loop_start() const;
    const SongTime& get_loop_end() const;
    
    // non-const accessors
    TrackIterator tracks_begin();
    TrackIterator tracks_end();
    TrackIterator tracks_find(int id);
    //@}

    /// @name Mutators
    //@{
    void set_title(const std::string& title);
    void set_author(const std::string& author);
    void set_info(const std::string& info);
    void set_length(const SongTime& length);
    TrackIterator add_track(const std::string& name = "");
    TrackIterator add_track(Track* track);
    bool remove_track(const TrackIterator& iterator);
    Track* disown_track(const TrackIterator& iterator);
    TempoIterator add_tempo_change(int beat, double bpm);
    void remove_tempo_change(TempoIterator& iter);
    void set_loop_start(const SongTime& start);
    void set_loop_end(const SongTime& end);
    //@}
    
    /// @name Sequencing
    //@{
    void get_timebase_info(unsigned long frame, unsigned long frame_rate,
         double& bpm, double& beat) const;
    double get_current_tempo(double beat) const;
    unsigned long bt2frame(double);
    std::pair<int, int> frame2bt(unsigned long frame);
    //@}
    
    /// @name XML I/O
    //@{
    bool is_dirty() const;
    bool write_file(const std::string& filename) const;
    bool load_file(const std::string& filename);
    void clear();
    //@}
    
  public:
    
    /// @name Signals
    //@{
    sigc::signal<void, const std::string&>& signal_title_changed() const;
    sigc::signal<void, const std::string&>& signal_author_changed() const;
    sigc::signal<void, const std::string&>& signal_info_changed() const;
    sigc::signal<void, const SongTime&>& signal_length_changed() const;
    sigc::signal<void, int>& signal_track_added() const;
    sigc::signal<void, int>& signal_track_removed() const;
    sigc::signal<void>& signal_tempo_changed() const;
    sigc::signal<void, const SongTime&>& signal_loop_start_changed() const;
    sigc::signal<void, const SongTime&>& signal_loop_end_changed() const;
    //@}
    
  private:
  
    // non-copyable for now
    Song& operator=(const Song&) { return *this; }
    Song(const Song&) { }
  
    std::string m_title;
    std::string m_author;
    std::string m_info;
    std::map<int, Track*>* volatile m_tracks;
    volatile SongTime m_length;
    
    TempoMap m_tempo_map;
    
    SongTime m_loop_start;
    SongTime m_loop_end;
    
    mutable bool m_dirty;

    mutable sigc::signal<void, const std::string&> m_signal_title_changed;
    mutable sigc::signal<void, const std::string&> m_signal_author_changed;
    mutable sigc::signal<void, const std::string&> m_signal_info_changed;
    mutable sigc::signal<void, const SongTime&> m_signal_length_changed;
    mutable sigc::signal<void, int> m_signal_track_added;
    mutable sigc::signal<void, int> m_signal_track_removed;
    mutable sigc::signal<void> m_signal_tempo_changed;
    mutable sigc::signal<void, const SongTime&> m_signal_loop_start_changed;
    mutable sigc::signal<void, const SongTime&> m_signal_loop_end_changed;
    
  };

}


#endif
