/*
    Copyright (C) 2013-2014 Sebastian Reichelt

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

#ifndef __libardour_pan_delay_buffer_h__
#define __libardour_pan_delay_buffer_h__

#include <cmath>

#include "ardour/session_handle.h"
#include "ardour/pan_distribution_buffer.h"

namespace ARDOUR {

class Session;

/** Buffer to add a delay to a panned channel.
 *
 * The delay is specified in the session properties, in ms/100%, where the
 * percentage refers to the difference between the two channels (for example,
 * L60R40 means 20% in this case). Only the position is relevant, not the
 * width of the stereo panner. The delay is applied to the output channel with
 * the lower percentage. (It might be nice if the width control affected the
 * phase differences of the incoming stereo signal, but that is a different
 * topic.)
 *
 * To keep things simple, the applied delay is always an integer number of
 * frames. As long as this integer stays the same, the implementation matches
 * a regular circular buffer. (We no longer use boost::circular_buffer because
 * it does not offer a way to preallocate memory beyond its capacity.) Things
 * become more complicated whenever the delay changes, as this requires
 * non-integer interpolation between the old and new delay, to avoid minor
 * clicks in the audio.
 */
class PanDelayBuffer : public PanDistributionBuffer, public SessionHandleRef
{
  public:
	PanDelayBuffer(Session &s);
	virtual ~PanDelayBuffer();

	/* Overridden to update _session_delay_coeff according to the delay
	 * specified in the session configuration. */
	virtual void update_session_config();

  protected:
	/* Overridden to update the delay according to the given panner
	 * position. */
	virtual void do_set_pan_position(float pan_position);

	/* Overridden to append the @a input sample to the delay buffer and
	 * remove and returns the oldest sample in the buffer. */
	virtual Sample do_process(Sample input);

	/* Overridden to honor delay. */
	virtual void do_mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain);

  private:
	/* The delay buffer, which is an array of size _buffer_size that is
	 * used as a circular buffer. */
	Sample *_buffer;

	/* Size of the _buffer array. */
	pframes_t _buffer_size;

	/* Position in the buffer where the next sample will be written.
	 * Increased by 1 for every sample, then wraps around at _buffer_size. */
	pframes_t _buffer_write_pos;

	/* Delay coefficient according to session configuration (in frames
	 * instead of ms). */
	float _session_delay_coeff;

	/* Current delay when interpolating. */
	float _current_delay;

	/* Desired delay; matches current delay if _interp_active is false. */
	pframes_t _desired_delay;

	/* Interpolation mode: See comment for _buffer. If true, _current_delay
	 * approaches _desired_delay in small steps; interpolation is finished
	 * as soon as they are equal. */
	bool _interp_active;

	/* Set to true on the first call to process() or an equivalent
	 * convenience method (and by update_session_config() if it returns
	 * false). As long as it is false, set_pan_position() sets the delay
	 * immediately without interpolation. */
	bool _samples_processed;

	/* Maximum delay, needed for memory preallocation. */
	static const float _max_delay_in_ms = 10.0f;

	/* Step size for _current_delay if _interp_active is true. */
	static const float _interp_inc = 1.0f / 16;

	/* Updates _session_delay_coeff and _active. */
	void update_session_delay_coeff();

	/* Called by do_process() if _interp_active is true. */
	Sample interpolate(Sample input);
};

} // namespace

#endif /* __libardour_pan_delay_buffer_h__ */
