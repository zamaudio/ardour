/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <windows.h>
#include <mmsystem.h>

#include <sstream>

#include "pbd/error.h"
#include "pbd/compose.h"

#include "winmmemidi_io.h"
#include "win_utils.h"
#include "debug.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace utils;

WinMMEMidiIO::WinMMEMidiIO()
	: m_active (false)
	, m_enabled (true)
	, m_run (false)
	, m_changed_callback (0)
	, m_changed_arg (0)
{
	pthread_mutex_init (&m_device_lock, 0);
}

WinMMEMidiIO::~WinMMEMidiIO()
{
	pthread_mutex_lock (&m_device_lock);
	cleanup();
	pthread_mutex_unlock (&m_device_lock);
	pthread_mutex_destroy (&m_device_lock);
}

void
WinMMEMidiIO::cleanup()
{
	DEBUG_MIDI ("MIDI cleanup\n");
	m_active = false;

	destroy_input_devices ();
	destroy_output_devices ();
}

bool
WinMMEMidiIO::dequeue_input_event (uint32_t port,
                                   uint64_t timestamp_start,
                                   uint64_t timestamp_end,
                                   uint64_t &timestamp,
                                   uint8_t *d,
                                   size_t &s)
{
	if (!m_active) {
		return false;
	}
	assert(port < m_inputs.size());

	// m_inputs access should be protected by trylock
	return m_inputs[port]->dequeue_midi_event (
	    timestamp_start, timestamp_end, timestamp, d, s);
}

bool
WinMMEMidiIO::enqueue_output_event (uint32_t port,
                                    uint64_t timestamp,
                                    const uint8_t *d,
                                    const size_t s)
{
	if (!m_active) {
		return false;
	}
	assert(port < m_outputs.size());

	// m_outputs access should be protected by trylock
	return m_outputs[port]->enqueue_midi_event (timestamp, d, s);
}


std::string
WinMMEMidiIO::port_id (uint32_t port, bool input)
{
	std::stringstream ss;

	if (input) {
		ss << "system:midi_capture_";
		ss << port;
	} else {
		ss << "system:midi_playback_";
		ss << port;
	}
	return ss.str();
}

std::string
WinMMEMidiIO::port_name (uint32_t port, bool input)
{
	if (input) {
		if (port < m_inputs.size ()) {
			return m_inputs[port]->name ();
		}
	} else {
		if (port < m_outputs.size ()) {
			return m_outputs[port]->name ();
		}
	}
	return "";
}

void
WinMMEMidiIO::start ()
{
	if (m_run) {
		DEBUG_MIDI ("MIDI driver already started\n");
		return;
	}

	m_run = true;
	DEBUG_MIDI ("Starting MIDI driver\n");

	set_min_timer_resolution();
	discover();
	start_devices ();
}


void
WinMMEMidiIO::stop ()
{
	DEBUG_MIDI ("Stopping MIDI driver\n");
	m_run = false;
	stop_devices ();
	pthread_mutex_lock (&m_device_lock);
	cleanup ();
	pthread_mutex_unlock (&m_device_lock);

	reset_timer_resolution();
}

void
WinMMEMidiIO::start_devices ()
{
	for (std::vector<WinMMEMidiInputDevice*>::iterator i = m_inputs.begin ();
	     i < m_inputs.end();
	     ++i) {
		if (!(*i)->start ()) {
			PBD::error << string_compose (_("Unable to start MIDI input device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
	for (std::vector<WinMMEMidiOutputDevice*>::iterator i = m_outputs.begin ();
	     i < m_outputs.end();
	     ++i) {
		if (!(*i)->start ()) {
			PBD::error << string_compose (_ ("Unable to start MIDI output device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
}

void
WinMMEMidiIO::stop_devices ()
{
	for (std::vector<WinMMEMidiInputDevice*>::iterator i = m_inputs.begin ();
	     i < m_inputs.end();
	     ++i) {
		if (!(*i)->stop ()) {
			PBD::error << string_compose (_ ("Unable to stop MIDI input device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
	for (std::vector<WinMMEMidiOutputDevice*>::iterator i = m_outputs.begin ();
	     i < m_outputs.end();
	     ++i) {
		if (!(*i)->stop ()) {
			PBD::error << string_compose (_ ("Unable to stop MIDI output device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
}

void
WinMMEMidiIO::create_input_devices ()
{
	int srcCount = midiInGetNumDevs ();

	DEBUG_MIDI (string_compose ("MidiIn count: %1\n", srcCount));

	for (int i = 0; i < srcCount; ++i) {
		try {
			WinMMEMidiInputDevice* midi_input = new WinMMEMidiInputDevice (i);
			if (midi_input) {
				m_inputs.push_back (midi_input);
			}
		}
		catch (...) {
			DEBUG_MIDI ("Unable to create MIDI input\n");
			continue;
		}
	}
}
void
WinMMEMidiIO::create_output_devices ()
{
	int dstCount = midiOutGetNumDevs ();

	DEBUG_MIDI (string_compose ("MidiOut count: %1\n", dstCount));

	for (int i = 0; i < dstCount; ++i) {
		try {
			WinMMEMidiOutputDevice* midi_output = new WinMMEMidiOutputDevice(i);
			if (midi_output) {
				m_outputs.push_back(midi_output);
			}
		} catch(...) {
			DEBUG_MIDI ("Unable to create MIDI output\n");
			continue;
		}
	}
}

void
WinMMEMidiIO::destroy_input_devices ()
{
	while (!m_inputs.empty ()) {
		WinMMEMidiInputDevice* midi_input = m_inputs.back ();
		// assert(midi_input->stopped ());
		m_inputs.pop_back ();
		delete midi_input;
	}
}

void
WinMMEMidiIO::destroy_output_devices ()
{
	while (!m_outputs.empty ()) {
		WinMMEMidiOutputDevice* midi_output = m_outputs.back ();
		// assert(midi_output->stopped ());
		m_outputs.pop_back ();
		delete midi_output;
	}
}

void
WinMMEMidiIO::discover()
{
	if (!m_run) {
		return;
	}

	if (pthread_mutex_trylock (&m_device_lock)) {
		return;
	}

	cleanup ();

	create_input_devices ();
	create_output_devices ();

	if (!(m_inputs.size () || m_outputs.size ())) {
		DEBUG_MIDI ("No midi inputs or outputs\n");
		pthread_mutex_unlock (&m_device_lock);
		return;
	}

	DEBUG_MIDI (string_compose ("Discovered %1 inputs and %2 outputs\n",
	                            m_inputs.size (),
	                            m_outputs.size ()));

	if (m_changed_callback) {
		m_changed_callback(m_changed_arg);
	}

	m_active = true;
	pthread_mutex_unlock (&m_device_lock);
}
