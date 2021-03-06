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

#ifndef OCTAVELABEL_HPP
#define OCTAVELABEL_HPP

#include <gtkmm.h>

#include "track.hpp"


class OctaveLabel : public Gtk::DrawingArea {
public:
  OctaveLabel(int width, int note_height);
  void set_track(const Dino::Track* track);

protected:
  
  virtual void on_realize();
  virtual bool on_expose_event(GdkEventExpose* event);

  void track_mode_changed(Dino::Track::Mode mode);  

  int m_width;
  int m_note_height;
  int m_drum_height;
  const Dino::Track* m_track;
  sigc::connection m_mode_conn;
  sigc::connection m_add_conn;
  sigc::connection m_remove_conn;
  sigc::connection m_change_conn;
  sigc::connection m_move_conn;
  
  Glib::RefPtr<Gdk::GC> m_gc;
  Glib::RefPtr<Gdk::Colormap> m_colormap;
  Gdk::Color m_bg_color, m_fg_color;
};


#endif
