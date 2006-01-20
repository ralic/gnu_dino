#include <sys/types.h>
#include <signal.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>

#include "debug.hpp"
#include "deleter.hpp"
#include "noteevent.hpp"
#include "pattern.hpp"


namespace Dino {
  
  
  Pattern::NoteIterator::NoteIterator() 
    : m_pattern(NULL),
      m_event(NULL) {

  }
  
  
  Pattern::NoteIterator::NoteIterator(const Pattern* pat, NoteEvent* event)
    : m_pattern(pat),
      m_event(event) {

  }
  
  
  const NoteEvent* Pattern::NoteIterator::operator*() const {
    return m_event;
  }
  
  
  const NoteEvent* Pattern::NoteIterator::operator->() const {
    return m_event;
  }
  
  
  bool Pattern::NoteIterator::operator==(const NoteIterator& iter) const {
    return (m_pattern == iter.m_pattern && m_event == iter.m_event);
  }
  
  
  bool Pattern::NoteIterator::operator!=(const NoteIterator& iter) const {
    return !operator==(iter);
  }
  
  
  Pattern::NoteIterator& Pattern::NoteIterator::operator++() {
    assert(m_pattern);
    assert(m_event);
    if (m_event->get_next())
      m_event = static_cast<NoteEvent*>(m_event->get_next());
    else {
      unsigned step = m_event->get_step();
      m_event = NULL;
      for (int i = step + 1; 
	   i < m_pattern->get_length() * m_pattern->get_steps(); ++i) {
	if (m_pattern->m_note_ons[i]) {
	  m_event = m_pattern->m_note_ons[i];
	  break;
	}
      }
    }
    
    return *this;
  }

  

  Pattern::Pattern(const string& name, int length, int steps) 
    : m_name(name), m_length(length), m_steps(steps), m_dirty(false) {
    dbg1<<"Creating pattern \""<<m_name<<"\""<<endl;
    assert(m_steps > 0);
    m_min_step = m_length * m_steps;
    m_max_step = -1;
    m_min_note = 128;
    m_max_note = -1;
    for (unsigned i = 0; i < m_length * m_steps; ++i) {
      m_note_ons.push_back(NULL);
      m_note_offs.push_back(NULL);
    }
    
    Controller c(m_length * m_steps, 1);
    c.set_event(0, 64);
    c.set_event(3, 3);
    c.set_event(14, 34);
    c.set_event(16, 95);
    c.set_event(24, 101);
    m_controllers.push_back(c);
  }


  Pattern::~Pattern() {
    dbg1<<"Destroying pattern \""<<m_name<<"\""<<endl;
    NoteEventList::iterator iter;
    for (iter = m_note_ons.begin(); iter != m_note_ons.end(); ++iter) {
      NoteEvent* event = *iter;
      while (event) {
	NoteEvent* tmp = event;
	delete event;
	event = static_cast<NoteEvent*>(tmp->get_next());
      }
    }
    for (iter = m_note_offs.begin(); iter != m_note_offs.end(); ++iter) {
      NoteEvent* event = *iter;
      while (event) {
	NoteEvent* tmp = event;
	delete event;
	event = static_cast<NoteEvent*>(tmp->get_next());
      }
    }
  }


  const string& Pattern::get_name() const {
    return m_name;
  }


  Pattern::NoteIterator Pattern::notes_begin() const {
    for (int i = 0; i < get_length() * get_steps(); ++i) {
      if (m_note_ons[i])
	return NoteIterator(this, m_note_ons[i]);
    }
    return NoteIterator(this, NULL);
  }

  
  Pattern::NoteIterator Pattern::notes_end() const {
    return NoteIterator(this, NULL);
  }


  const vector<Controller>& Pattern::get_controllers() const {
    return m_controllers;
  }


  void Pattern::set_name(const string& name) {
    if (name != m_name) {
      dbg1<<"Changing pattern name from \""<<m_name<<"\" to \""<<name<<"\""<<endl;
      m_name = name;
      signal_name_changed(m_name);
    }
  }


  /** This function tries to add a new note with the given parameters. If 
      there is another note starting at the same step and with the same value,
      the old note will be deleted. If there is a note that is playing at that
      step with the same value, the old note will be shortened so it ends just
      before @c step. This function will also update the @c previous and 
      @c next pointers in the Note, so the doubly linked list will stay
      consistent with the note map. */
  void Pattern::add_note(unsigned step, int value, int velocity, 
			 int noteLength) {
    assert(step >= 0);
    assert(step < m_length * m_steps);
    assert(value >= 0);
    assert(value < 128);
  
    NoteEvent* note_on;
    NoteEvent* note_off;
  
    // if a note with this value is playing at this step, stop it
    NoteEvent* playing_note = find_note_internal(step, value);
    if (playing_note) {
      if (playing_note->get_step() == (unsigned int)step) {
	NoteEvent* assoc = playing_note->get_assoc();
	delete_note_event(playing_note);
	delete_note_event(assoc);
	signal_note_removed(step, value);
      }
      else {
	NoteEvent* old_off = playing_note->get_assoc();
	NoteEvent* new_off = new NoteEvent(MIDIEvent::NoteOff, step - 1, 
					   value, 64, 0, playing_note);
	add_note_event(new_off);
	playing_note->set_assoc(new_off);
	playing_note->set_length(step - playing_note->get_step());
	delete_note_event(old_off);
	signal_note_changed(playing_note->get_step(), value, 
			    playing_note->get_length());
      }
    }
  
    // make sure that the new note fits
    int newLength = noteLength;
    for (unsigned i = step + 1; i < step + noteLength; ++i) {
      if (find_note_event(i, value, true, playing_note)) {
	newLength = i - step;
	break;
      }
    }
  
    // add the note events
    note_off = 
      new NoteEvent(NoteEvent::NoteOff, step + newLength - 1, 
		    value, velocity, 0);
    add_note_event(note_off);
    note_on = 
      new NoteEvent(NoteEvent::NoteOn, step, 
		    value, velocity, newLength, note_off);
    note_off->set_assoc(note_on);
    add_note_event(note_on);
  
    signal_note_added(step, value, newLength);
  }


  /** This function will delete the note with the given value playing at the
      given step, if there is one. It will also update the @c previous and 
      @c next pointers so the doubly linked list will stay consistent with the
      note map. It will return the step that the deleted note started on,
      or -1 if no note was deleted. */
  int Pattern::delete_note(NoteIterator iterator) {
    assert(iterator);
    assert(iterator.m_pattern == this);
    NoteEvent* note_on = const_cast<NoteEvent*>(iterator.m_event);
    
    if (note_on) {
      NoteEvent* off = note_on->get_assoc();
      int on_step = note_on->get_step();
      unsigned char value = note_on->get_note();
      delete_note_event(note_on);
      delete_note_event(off);
      signal_note_removed(on_step, value);
      return on_step;
    }
    
    return -1;
  }


  int Pattern::resize_note(NoteIterator iterator, int length) {
    assert(iterator);
    assert(iterator.m_pattern == this);
    NoteEvent* note_on = const_cast<NoteEvent*>(iterator.m_event);
    
    unsigned int step = note_on->get_step();
  
    if (length < 1)
      length = 1;
    if (note_on->get_step() + length >= (unsigned int)(m_length * m_steps))
      length = m_length * m_steps - note_on->get_step();
  
    // check that the note will not overlap with another note
    for (unsigned int i = step + 1; i < step + length; ++i) {
      NoteEvent* event;
      if (find_note_event(i, note_on->get_note(), true, event)) {
	length = i - step;
	break;
      }
    }
  
    // resize the note
    if ((unsigned int)length != note_on->get_length()) {
      NoteEvent* new_off = new NoteEvent(NoteEvent::NoteOff, step + length - 1, 
					 note_on->get_note(), 64, 0, note_on);
      NoteEvent* old_off = note_on->get_assoc();
      add_note_event(new_off);
      note_on->set_assoc(new_off);
      note_on->set_length(length);
      delete_note_event(old_off);
      signal_note_changed(step, note_on->get_note(), length);
    }
    return length;
  }


  void Pattern::set_velocity(NoteIterator note, unsigned char velocity) {
    assert(note);
    assert(note.m_pattern == this);
    assert(velocity < 128);
    note.m_event->set_velocity(velocity);
  }


  void Pattern::add_cc(unsigned int controller, unsigned int step, 
		       unsigned char value) {
    assert(controller < m_controllers.size());
    assert(step < m_length * m_steps);
    m_controllers[controller].set_event(step, value);
  }


  void Pattern::remove_cc(unsigned int controller, unsigned int step) {
    
  }

  
  int Pattern::get_steps() const {
    return m_steps;
  }


  int Pattern::get_length() const {
    return m_length;
  }
  

  void Pattern::get_dirty_rect(int* minStep, int* minNote, 
			       int* maxStep, int* maxNote) const {
    if (minStep)
      *minStep = this->m_min_step;
    if (minNote)
      *minNote = this->m_min_note;
    if (maxStep)
      *maxStep = this->m_max_step;
    if (maxNote)
      *maxNote = this->m_max_note;
  }
  
  
  void Pattern::reset_dirty_rect() {
    m_min_step = m_length * m_steps;
    m_max_step = -1;
    m_min_note = 128;
    m_max_note = -1;
  }

  
  bool Pattern::is_dirty() const {
    return m_dirty;
  }


  void Pattern::make_clean() const {
    m_dirty = false;
  }


  bool Pattern::fill_xml_node(Element* elt) const {
    char tmp_txt[10];
    elt->set_attribute("name", m_name);
    sprintf(tmp_txt, "%d", get_length());
    elt->set_attribute("length", tmp_txt);
    sprintf(tmp_txt, "%d", get_steps());
    elt->set_attribute("steps", tmp_txt);
    for (unsigned int i = 0; i < m_note_ons.size(); ++i) {
      NoteEvent* ne = m_note_ons[i];
      while (ne) {
	if (ne->get_type() == NoteEvent::NoteOn) {
	  Element* note_elt = elt->add_child("note");
	  sprintf(tmp_txt, "%d", ne->get_step());
	  note_elt->set_attribute("step", tmp_txt);
	  sprintf(tmp_txt, "%d", ne->get_note());
	  note_elt->set_attribute("value", tmp_txt);
	  sprintf(tmp_txt, "%d", ne->get_length());
	  note_elt->set_attribute("length", tmp_txt);
	  sprintf(tmp_txt, "%d", ne->get_velocity());
	  note_elt->set_attribute("velocity", tmp_txt);
	}
	ne = static_cast<NoteEvent*>(ne->get_next());
      }
    }
    return true;
  }


  bool Pattern::parse_xml_node(const Element* elt) {
    m_name = "";
    Node::NodeList nodes = elt->get_children("name");
    if (nodes.begin() != nodes.end()) {
      const Element* name_elt = dynamic_cast<const Element*>(*nodes.begin());
      if (name_elt) {
	m_name = name_elt->get_child_text()->get_content();
	signal_name_changed(m_name);
      }
    }

    nodes = elt->get_children("note");
    Node::NodeList::const_iterator iter;
    for (iter = nodes.begin(); iter != nodes.end(); ++iter) {
      const Element* note_elt = dynamic_cast<const Element*>(*iter);
      if (!note_elt)
	continue;
      int step, value, length, velocity;
      sscanf(note_elt->get_attribute("step")->get_value().c_str(), "%d", &step);
      sscanf(note_elt->get_attribute("value")->get_value().c_str(), 
	     "%d", &value);
      sscanf(note_elt->get_attribute("length")->get_value().c_str(), 
	     "%d", &length);
      velocity = 64;
      if (note_elt->get_attribute("velocity")) {
	sscanf(note_elt->get_attribute("velocity")->get_value().c_str(), 
	       "%d", &velocity);
      }
      add_note(step, value, velocity, length);
    }
    return true;
  }


  MIDIEvent* Pattern::get_events(unsigned int& beat, unsigned int& tick, 
				     unsigned int before_beat, 
				     unsigned int before_tick,
				     unsigned int ticks_per_beat,
				     unsigned int& list) const {
    
    int steps = m_steps;
    // convert beats and ticks to pattern steps
    double beat_d = tick / double(ticks_per_beat);
    unsigned int step = (unsigned int)ceil((beat + beat_d) * steps);
    double b_beat_d = before_tick / double(ticks_per_beat);
    unsigned int b_step = (unsigned int)ceil((before_beat + b_beat_d) * steps);
    unsigned int i = step;
  
    for ( ; i < b_step && i < (unsigned long)(m_length * steps); ++i) {
    
      //cerr<<"list = "<<list<<", i = "<<i<<endl;
      
      if (list == 0) {
	list = 1;
	if (m_note_ons[i]) {
	  beat = i / steps;
	  tick = (unsigned int)((i % steps) * ticks_per_beat / double(steps));
	  m_already_returned = true;
	  m_last_beat = beat;
	  m_last_tick = tick;
	  return m_note_ons[i];
	}
      }
      
      if (list == 1) {
	list = 2;
	if (m_note_offs[i]) {
	  beat = i / steps;
	  tick = (unsigned int)((i % steps) * ticks_per_beat / double(steps));
	  m_already_returned = true;
	  m_last_beat = beat;
	  m_last_tick = tick;
	  return m_note_offs[i];
	}
      }
      
      if (list <= 2) {
	list = 3;
	/*
	  const EventList& m_ccs = m_control_changes.find(1)->second;
	  if (m_ccs[i]) {
	  beat = i / steps;
	  tick = (unsigned int)((i % steps) * ticks_per_beat / double(steps));
	  m_already_returned = true;
	  m_last_beat = beat;
	  m_last_tick = tick;
	  cerr<<"returning CC at beat "<<beat<<", tick "<<tick<<endl;
	  return m_ccs[i];
	  }
	*/
      }
      
      list = 0;
    }
    
    beat = before_beat;
    tick = before_tick;
  
    return NULL;
  }


  int Pattern::get_events2(unsigned int& beat, unsigned int& tick,
			   unsigned int before_beat, unsigned int before_tick,
			   unsigned int ticks_per_beat, 
			   MIDIEvent** events, int room) const {
    return 0;
  }
  

  Pattern::NoteIterator Pattern::find_note(int step, int value) const {
    NoteEvent* event;
  
    for (int i = step; i >= 0; --i) {

      if (i < step) {
	event = m_note_offs[i];
	while (event) {
	  if (event->get_note() == value)
	    return NoteIterator(this, NULL);
	  event = static_cast<NoteEvent*>(event->get_next());
	}
      }
    
      event = m_note_ons[i];
      while (event) {
	if (event->get_note() == value)
	  return NoteIterator(this, event);
	event = static_cast<NoteEvent*>(event->get_next());
      }
    }
  
    return NoteIterator(this, NULL);
  }


  NoteEvent* Pattern::find_note_internal(int step, int value) {
    NoteEvent* event;
  
    for (int i = step; i >= 0; --i) {

      if (i < step) {
	event = m_note_offs[i];
	while (event) {
	  if (event->get_note() == value)
	    return NULL;
	  event = static_cast<NoteEvent*>(event->get_next());
	}
      }
    
      event = m_note_ons[i];
      while (event) {
	if (event->get_note() == value)
	  return event;
	event = static_cast<NoteEvent*>(event->get_next());
      }
    }
  
    return NULL;
  }


  bool Pattern::find_note_event(int step, int value, bool note_on, 
				NoteEvent*& event) {
    if (note_on)
      event = m_note_ons[step];
    else
      event = m_note_offs[step];
    while (event) {
      if (event->get_note() == value && event->get_type() == NoteEvent::NoteOn)
	return true;
      event = static_cast<NoteEvent*>(event->get_next());
    }
    return false;
  }


  void Pattern::delete_note_event(NoteEvent* event) {
    NoteEventList* notes;
    if (event->get_type() == MIDIEvent::NoteOn)
      notes = &m_note_ons;
    else
      notes = &m_note_offs;
  
    unsigned int step = event->get_step();
    if (!event->get_previous())
      (*notes)[step] = static_cast<NoteEvent*>(event->get_next());
    else
      event->get_previous()->set_next(event->get_next());
    if (event->get_next())
      static_cast<NoteEvent*>(event->get_next())->
	set_previous(event->get_previous());
    event->queue_deletion();
  }
  

  void Pattern::add_note_event(NoteEvent* event) {
  
    NoteEventList* notes;
    if (event->get_type() == NoteEvent::NoteOn)
      notes = &m_note_ons;
    else
      notes = &m_note_offs;
  
    unsigned int step = event->get_step();
    event->set_previous(NULL);
    event->set_next((*notes)[step]);
    if ((*notes)[step])
      (*notes)[step]->set_previous(event);
    (*notes)[step] = event;
  }


}
