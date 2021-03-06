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

#include <cassert>
#include <iostream>
#include <map>

#include "tracklabel.hpp"
#include "track.hpp"


using namespace std;
using namespace Dino;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace Pango;


TrackLabel::TrackLabel(const Song* song) 
  : m_song(song), 
    m_width(122), 
    m_height(20),
    m_cce_height(68),
    m_is_active(false),
    m_is_recording(false),
    m_has_curves(false) {
  
  assert(song);
  m_colormap  = Colormap::get_system();
  m_bg_color.set_rgb(30000, 30000, 60000);
  m_fg_color.set_rgb(0, 0, 0);
  m_colormap->alloc_color(m_bg_color);
  m_colormap->alloc_color(m_fg_color);
  m_layout = Pango::Layout::create(get_pango_context());

  add_events(BUTTON_PRESS_MASK | BUTTON_RELEASE_MASK | BUTTON_MOTION_MASK);
  set_size_request(m_width, m_height + 4);
  
  m_kb_icon = Pixbuf::create_from_file(DATA_DIR "/tinykeyboard.png");
}
  

void TrackLabel::set_track(int id, const Track* track) {

  m_name_connection.disconnect();
  m_ctrl_added_connection.disconnect();
  m_ctrl_removed_connection.disconnect();
  
  assert(track);
  
  m_track = track;
  m_id = id;
  
  m_name_connection = m_track->signal_name_changed().
    connect(mem_fun(*this, &TrackLabel::slot_name_changed));
  m_ctrl_added_connection = track->signal_curve_added().
    connect(mem_fun(*this, &TrackLabel::curve_added));
  m_ctrl_removed_connection = track->signal_curve_removed().
    connect(mem_fun(*this, &TrackLabel::curve_removed));
  
  slot_name_changed(track->get_name());
  
  m_has_curves = (track->curves_begin() != track->curves_end());
  
  set_size_request(m_width, m_height + 4 + (m_has_curves ? m_cce_height : 0));
}


void TrackLabel::on_realize() {
  DrawingArea::on_realize();
  RefPtr<Gdk::Window> win = get_window();
  m_gc = GC::create(win);
  m_gc->set_background(m_bg_color);
  m_gc->set_foreground(m_fg_color);
  FontDescription fd("helvetica bold 9");
  get_pango_context()->set_font_description(fd);
  win->clear();
}


bool TrackLabel::on_expose_event(GdkEventExpose* event) {
  RefPtr<Gdk::Window> win = get_window();
  win->clear();
  if (m_is_active) {
    m_gc->set_foreground(m_bg_color);
    int x, y, w, h, d;
    win->get_geometry(x, y, w, h, d);
    win->draw_rectangle(m_gc, true, 0, 4, w, h - 4);
  }
  
  if (m_is_recording)
    win->draw_pixbuf(m_gc, m_kb_icon, 0, 0, 2, 4 + (m_height) / 2 - 8,
		     18, 16, RGB_DITHER_NONE, 0, 0);
  
  m_gc->set_foreground(m_fg_color);
  int lHeight = m_layout->get_pixel_logical_extents().get_height();
  win->draw_layout(m_gc, 24, 4 + (m_height - lHeight)/2, m_layout);
  return true;
}


bool TrackLabel::on_button_press_event(GdkEventButton* event) {
  signal_clicked(event->button);
  return true;
}


bool TrackLabel::on_button_release_event(GdkEventButton* event) {
  return true;
}


bool TrackLabel::on_motion_notify_event(GdkEventMotion* event) {
  return true;
}


void TrackLabel::set_active_track(int id) {
  if (m_is_active != (id == m_id)) {
    m_is_active = (id == m_id);
    queue_draw();
  }
}


void TrackLabel::set_recording(bool recording) {
  m_is_recording = recording;
  queue_draw();
}


void TrackLabel::slot_name_changed(const string& name) {
  char tmp[10];
  sprintf(tmp, "%03d ", m_id);
  m_layout->set_text(string(tmp) + name);
  queue_draw();
}


void TrackLabel::curve_added(long number) {
  m_has_curves = (m_track->curves_begin() != m_track->curves_end());
  set_size_request(m_width, m_height + 4 + (m_has_curves ? m_cce_height : 0));
}


void TrackLabel::curve_removed(long number) {
  m_has_curves = (m_track->curves_begin() != m_track->curves_end());
  set_size_request(m_width, m_height + 4 + (m_has_curves ? m_cce_height : 0));  
}

