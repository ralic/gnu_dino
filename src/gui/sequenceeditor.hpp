/****************************************************************************
   Dino - A simple pattern based MIDI sequencer
   
   Copyright (C) 2006  Lars Luthman <larsl@users.sourceforge.net>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation, 
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
****************************************************************************/

#ifndef SEQUENCEEDITOR_HPP
#define SEQUENCEEDITOR_HPP

#include <map>

#include <gtkmm.h>

#include "plugininterface.hpp"
#include "debug.hpp"
#include "ruler.hpp"
#include "trackdialog.hpp"

namespace Dino {
  class Sequencer;
  class Song;
}


class PluginInterface;


class SequenceEditor : public GUIPage {
public:
  
  SequenceEditor(PluginInterface& plif);
  
  void reset_gui();
  
protected:
  
  void add_track();
  void delete_track();
  void edit_track_properties();
  void play();
  void stop();
  void go_to_start();
  void ruler_clicked(double beat, int button);
  
  void set_active_track(int track);
  
  void add_toolbutton(Gtk::Toolbar* tbar, Gtk::ToolButton*& tbutton, 
                      Gtk::BuiltinStockID stock, const std::string& tip,
                      void (SequenceEditor::*button_slot)());

  sigc::signal<void, int> signal_active_track_changed_internal;


  Ruler m_sequence_ruler;
  Gtk::VBox* m_vbx_track_editor;
  Gtk::VBox* m_vbx_track_labels;
  Gtk::ToolButton* m_tbn_add_track;
  Gtk::ToolButton* m_tbn_delete_track;
  Gtk::ToolButton* m_tbn_edit_track_properties;
  Gtk::ToolButton* m_tbn_play;
  Gtk::ToolButton* m_tbn_stop;
  Gtk::ToolButton* m_tbn_go_to_start;
  Gtk::SpinButton* m_spb_song_length;
  TrackDialog* m_dlg_track;
  Gtk::Tooltips m_tooltips;
  
  int m_active_track;
  
  Dino::Song& m_song;
  Dino::Sequencer& m_seq;
  PluginInterface& m_plif;
};


#endif
