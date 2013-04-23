/*
    Copyright (C) 2006, 2013 Paul Davis
    Author: Robin Gareus

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/


// TODO use PBD::DEBUG
//#define DEBUG_LATENCY_COMPENSATION_DELAYLINE

#include <assert.h>
#include <cmath>

#include "pbd/compose.h"

#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/delayline.h"

using namespace std;

using namespace ARDOUR;

DelayLine::DelayLine (Session& s, const std::string& name)
    : Processor (s, string_compose ("latency-compensation-%1", name))
		, _delay(0)
		, _pending_delay(0)
		, _bsiz(0)
		, _pending_bsiz(0)
		, _roff(0)
		, _woff(0)
{
}

DelayLine::~DelayLine ()
{
}

#define FADE_LEN (16)
void
DelayLine::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nsamples, bool)
{
	const uint32_t chn = _configured_output.n_audio();
	pframes_t p0 = 0;
	int c;

	/* run() and set_delay() may be called in parallel by
	 * different threads.
	 * if a larger buffer is needed, it is allocated in
	 * set_delay(), we just swap it in place
	 */
	if (_pending_bsiz)  {
		int boff = _pending_bsiz - _bsiz;

		if (_bsiz > 0) {
			// TODO copy/wrap buffer if it grows > (_woff-_roff)
			memcpy(_pending_buf.get() + boff * chn, _buf.get(), sizeof(Sample) * _bsiz * chn);
		}

		if (_roff > _woff ) {
			// TODO copy/wrap buffer -- and/or postpone /fade in/
			// apply fade to existing buffer..
			_roff += boff;
		}

		_buf = _pending_buf;
		_bsiz = _pending_bsiz;
		_pending_bsiz = 0;
	}

	/* initially there may be no buffer -- delay == 0 */
	Sample *buf = _buf.get();
	if (!buf) { return; }

	assert (_bsiz >= _pending_delay);
	const framecnt_t rbs = _bsiz + 1;

	if (_pending_delay != _delay) {
		const pframes_t fade_len = (nsamples >= FADE_LEN) ? FADE_LEN : nsamples / 2;

#ifdef DEBUG_LATENCY_COMPENSATION_DELAYLINE
		cerr << "Old " << name()
			<< " delay: " << _delay
			<< " bufsiz: " << _bsiz
			<< " offset-diff: " << ((_woff - _roff + rbs) % rbs)
			<< " write-offset: " << _woff
			<< " read-offset:" << _roff
			<< "\n";
#endif

		// fade out at old position
		c = 0;
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
			Sample * const data = i->data();
			for (pframes_t pos = 0; pos < fade_len; ++pos) {
				const gain_t gain = (gain_t)(fade_len - pos) / (gain_t)fade_len;
				buf[ _woff * chn + c ] = data[ pos ];
				data[ pos ] = buf[ _roff * chn + c ] * gain;
				_roff = (_roff + 1) % rbs;
				_woff = (_woff + 1) % rbs;
			}
		}

		// adjust read pointer
		_roff += _delay - _pending_delay;

		if (_roff < 0) {
			_roff -= rbs * floor(_roff / (float)rbs);
		}
		_roff = _roff % rbs;

		// fade in at new position
		c = 0;
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
			Sample * const data = i->data();
			for (pframes_t pos = fade_len; pos < 2 * fade_len; ++pos) {
				const gain_t gain = (gain_t)(pos - fade_len) / (gain_t)fade_len;
				buf[ _woff * chn + c ] = data[ pos ];
				data[ pos ] = buf[ _roff * chn + c ] * gain;
				_roff = (_roff + 1) % rbs;
				_woff = (_woff + 1) % rbs;
			}
		}
		p0  = 2 * fade_len;

		_delay = _pending_delay;
#ifdef DEBUG_LATENCY_COMPENSATION_DELAYLINE
		cerr << "New " << name()
			<< " delay: " << _delay
			<< " bufsiz: " << _bsiz
			<< " offset-diff: " << ((_woff - _roff + rbs) % rbs)
			<< " write-offset: " << _woff
			<< " read-offset:" << _roff
			<< "\n";
#endif
	}

	assert(_delay == ((_woff - _roff + rbs) % rbs));

	c = 0;
	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++c) {
		Sample * const data = i->data();
		for (pframes_t pos = p0; pos < nsamples; ++pos) {
			buf[ _woff * chn + c ] = data[ pos ];
			data[ pos ] = buf[ _roff * chn + c ];
			_roff = (_roff + 1) % rbs;
			_woff = (_woff + 1) % rbs;
		}
	}

	for (BufferSet::midi_iterator i = bufs.midi_begin(); i != bufs.midi_end(); ++i) {
		; // TODO
	}
}

void
DelayLine::set_delay(framecnt_t signal_delay)
{
#ifdef DEBUG_LATENCY_COMPENSATION_DELAYLINE
	cerr << "set delay of " << name() << " to " << signal_delay << " samples for "<< _configured_output.n_audio() << " channels.\n";
#endif
	const framecnt_t rbs = signal_delay + 1;

	if (signal_delay <= _bsiz) {
		_pending_delay = signal_delay;
		return;
	}

	if (_pending_bsiz) {
		if (_pending_bsiz < signal_delay) {
			cerr << "buffer resize in progress. "<< name() << "pending: "<< _pending_bsiz <<" want: " << signal_delay <<"\n";
		} else {
			_pending_delay = signal_delay;
		}
		return;
	}

	_pending_buf.reset(new Sample[_configured_output.n_audio() * rbs]);
	_pending_delay = signal_delay;
	_pending_bsiz = signal_delay;
#ifdef DEBUG_LATENCY_COMPENSATION_DELAYLINE
	cerr << "allocated buffer for " << name() << " of " << signal_delay << " samples\n";
#endif
}

bool
DelayLine::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
DelayLine::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}

#ifdef DEBUG_LATENCY_COMPENSATION_DELAYLINE
	cerr << "configure IO: " << name()
		<< " Ain: " << in.n_audio()
		<< " Aout: " << out.n_audio()
		<< " Min: " << in.n_midi()
		<< " Mout: " << out.n_midi()
		<< "\n";
#endif

	return Processor::configure_io (in, out);
}

void
DelayLine::monitoring_changed()
{
}

XMLNode&
DelayLine::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "delay");
	return node;
}
