#ifndef DINOGUI_HPP
#define DINOGUI_HPP

#include <gtkmm.h>
#include <libglademm.h>
#include <lash/lash.h>

#include "cceditor.hpp"
#include "octavelabel.hpp"
#include "patterneditor.hpp"
#include "ringbuffer.hpp"
#include "ruler.hpp"
#include "sequencer.hpp"
#include "singletextcombo.hpp"
#include "song.hpp"


using namespace Glib;
using namespace Gnome::Glade;
using namespace Gtk;
using namespace sigc;
using namespace std;
using namespace Dino;


/** This is the main class. It connects our custom widgets to the rest of the
    GUI and sets up all signals and initial values. */
class DinoGUI {
public:
  DinoGUI(int argc, char** argv, RefPtr<Xml> xml);
  
  Gtk::Window* get_window();
  
  // menu and toolbutton callbacks
  void slot_file_new();
  void slot_file_open();
  void slot_file_save();
  void slot_file_save_as();
  void slot_file_quit();
  
  void slot_edit_cut();
  void slot_edit_copy();
  void slot_edit_paste();
  void slot_edit_delete();
  void slot_edit_add_track();
  void slot_edit_delete_track();
  void slot_edit_edit_track_properties();
  void slot_edit_add_pattern();
  void slot_edit_delete_pattern();
  void slot_edit_edit_pattern_properties();
  void slot_edit_add_controller();
  void slot_edit_delete_controller();
  void slot_edit_set_song_length();
  
  void slot_transport_play();
  void slot_transport_stop();
  void slot_transport_go_to_start();
  
  void slot_help_about_dino();
  
private:
  
  /** This is a convenience function that returns a pointer of type @c T* to
      the widget with name @c name. If there is no widget in @c xml with that
      name it returns NULL. */
  template <class T>
  inline T* w(const string& name) {
    return dynamic_cast<T*>(m_xml->get_widget(name));
  }
  
  void reset_gui();
  void update_track_widgets();
  void update_track_combo();
  void update_pattern_combo();
  void update_controller_combo();
  void update_editor_widgets();
  void update_port_combo();
  void init_pattern_editor();
  void init_sequence_editor();
  void init_info_editor();
  void init_menus();
  bool init_lash(int argc, char** argv);
  
  // internal callbacks
  void slot_cc_number_changed();
  void slot_cc_editor_size_changed();
  bool slot_check_ladcca_events();
  void set_active_track(int active_track);
  void set_active_pattern(int active_pattern);
  void set_active_controller(int active_controller);
  
  // internal signals
  signal<void, int> signal_active_track_changed;
  signal<void, int, int> signal_active_pattern_changed;
  signal<void, int> signal_active_controller_changed;
  
  Song m_song;
  int m_active_track;
  int m_active_pattern;
  int m_active_controller;
  
  RefPtr<Xml> m_xml;
  Gtk::Window* m_window;
  PatternEditor m_pe;
  CCEditor m_cce;
  VBox* m_vbx_track_editor;
  VBox* m_vbx_track_labels;
  
  SingleTextCombo m_cmb_track;
  SingleTextCombo m_cmb_pattern;
  SingleTextCombo m_cmb_controller;
  connection m_track_combo_connection;
  connection m_pattern_combo_connection;
  connection m_conn_pat_added;
  connection m_conn_pat_removed;
  connection m_conn_cont_added;
  connection m_conn_cont_removed;
  //SpinButton* m_sb_cc_number;
  //Label* m_lb_cc_description;
  SpinButton* m_sb_cc_editor_size;
  ::Ruler m_sequence_ruler;
  PatternRuler m_pattern_ruler_1;
  OctaveLabel m_octave_label;
  Entry* m_ent_title;
  Entry* m_ent_author;
  TextView* m_text_info;
  
  Dialog* m_dlg_track_properties;
  Entry* m_dlgtrack_ent_name;
  SingleTextCombo m_dlgtrack_cmb_port;
  SpinButton* m_dlgtrack_sbn_channel;
  
  Dialog* m_dlg_pattern_properties;
  Entry* m_dlgpat_ent_name;
  SpinButton* m_dlgpat_sbn_length;
  SpinButton* m_dlgpat_sbn_steps;
  SpinButton* m_dlgpat_sbn_cc_steps;
  
  Dialog* m_dlg_controller_properties;
  Entry* m_dlgcont_ent_name;
  SingleTextCombo m_dlgcont_cmb_controller;
  
  static char* cc_descriptions[];
  
  Dialog* m_about_dialog;
  
  Sequencer m_seq;
  
  lash_client_t* m_lash_client;
};


#endif


// For the Doxygen main page:
/** @mainpage Dino source code documentation
    This source code documentation is generated in order to help people
    (including me) understand how Dino works, or is supposed to work. 
    But it's not meant to be a complete API reference, so many functions and
    types will be undocumented.
    
    Send questions and comments to larsl@users.sourceforge.net. 
*/
