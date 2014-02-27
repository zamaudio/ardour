/*
    Copyright (C) 2014 Sebastian Reichelt

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ardour/pan_distribution_buffer.h"
#include "ardour/runtime_functions.h"

using namespace std;
using namespace ARDOUR;

const pframes_t PanDistributionBuffer::_gain_interp_frames = 64;

PanDistributionBuffer::PanDistributionBuffer()
	: _active(false)
{
}

void
PanDistributionBuffer::mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float prev_gain, float gain)
{
	if (nframes == 0) {
		return;
	}

	if (gain == prev_gain) {
		do_mix_buffers(dst, src, nframes, gain);
	} else {
		/* gain has changed, so we must interpolate over 64 frames or nframes, whichever is smaller */
		/* (code adapted from panner_1in2out.cc and panner_2in2out.cc) */

		pframes_t const limit = min(_gain_interp_frames, nframes);
		float const delta = (gain - prev_gain) / limit;
		float current_gain = prev_gain;
		pframes_t n = 0;

		for (; n < limit; n++) {
			prev_gain += delta;
			current_gain = prev_gain + 0.9 * (current_gain - prev_gain);
			dst[n] += process(src[n] * current_gain);
		}

		if (n < nframes) {
			do_mix_buffers(dst + n, src + n, nframes - n, gain);
		}
	}
}

void
PanDistributionBuffer::do_mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain)
{
	if (gain == 1.0f) {
		/* gain is 1 so we can just copy the input samples straight in */
		mix_buffers_no_gain(dst, src, nframes);
	} else if (gain != 0.0f) {
		/* gain is not 1 but also not 0, so we must do it "properly" */
		mix_buffers_with_gain(dst, src, nframes, gain);
	}
}
