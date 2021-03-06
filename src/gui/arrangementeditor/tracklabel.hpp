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

#ifndef TRACKLABEL_HPP
#define TRACKLABEL_HPP

#include <string>

#include <gtkmm.h>


namespace Dino {
  class Song;
  class Track;
}


class TrackLabel : public Gtk::DrawingArea {
public:
  
  TrackLabel(const Dino::Song* song = 0);
  
  void set_track(int id, const Dino::Track* track);
  
  virtual void on_realize();
  virtual bool on_expose_event(GdkEventExpose* event);
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_button_release_event(GdkEventButton* event);
  virtual bool on_motion_notify_event(GdkEventMotion* event);
  
  void set_active_track(int id);
  void set_recording(bool recording);
  
  sigc::signal<void, int> signal_clicked;
  
private:
  
  void slot_name_changed(const std::string& name);
  void curve_added(long number);
  void curve_removed(long number);
    
  const Dino::Song* m_song;
  const Dino::Track* m_track;
  sigc::connection m_name_connection;
  int m_id;
  int m_width, m_height;
  int m_cce_height;
  bool m_is_active;
  bool m_is_recording;
  bool m_has_curves;

  Glib::RefPtr<Gdk::GC> m_gc;
  Glib::RefPtr<Gdk::Colormap> m_colormap;
  Gdk::Color m_bg_color, m_fg_color;
  Glib::RefPtr<Pango::Layout> m_layout;
  Glib::RefPtr<Gdk::Pixbuf> m_kb_icon;

  sigc::connection m_ctrl_added_connection;
  sigc::connection m_ctrl_removed_connection;

};


#endif
