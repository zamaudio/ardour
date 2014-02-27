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

#ifndef __libardour_pan_distribution_buffer_h__
#define __libardour_pan_distribution_buffer_h__

#include "ardour/types.h"

namespace ARDOUR {

/** Helper class for panners to manage distribution of signals to outputs.
 *
 * The only method in this class that actually does something interesting
 * is mix_buffers(). All others exist purely to be overridden by subclasses.
 * (There is currently just one subclass called PanDelayBuffer.)
 *
 * Clients should call update_session_config() whenever the session
 * configuration might have changed, then set_pan_position() whenever the
 * position of the panner might have changed, and then process() for every
 * sample. For convenience and performance, the two helper methods
 * set_pan_position_and_process() and mix_buffers() can be used instead.
 *
 * Since set_pan_position() and process() are potentially called for each
 * sample, their most common case is inlined. Subclasses can make sure that
 * this inlined code is used by setting _active to false.
 *
 * For more information, see pan_delay_buffer.h.
 */
class PanDistributionBuffer
{
  public:
	PanDistributionBuffer();
	virtual ~PanDistributionBuffer();

	/** Updates internal data according to the session configuration. */
	virtual void update_session_config();

	/** Updates internal data according to the given panner position.
	 *
	 * @a pan_position should be a value between 0 and 1, and should not
	 * be a gain value that has been calculated according to the pan law.
	 * For a stereo output, the @a pan_position values of the left and
	 * right channel should sum to 1. */
	void set_pan_position(float pan_position)
	{
		if (_active) {
			do_set_pan_position(pan_position);
		}
	}

	/** Processes one sample, and returns the sample that should actually
	 *  be output. */
	Sample process(Sample input)
	{
		if (_active) {
			return do_process(input);
		} else {
			return input;
		}
	}

	/** Same as set_pan_position() followed by process(). */
	Sample set_pan_position_and_process(float pan_position, Sample input)
	{
		if (_active) {
			do_set_pan_position(pan_position);
			return do_process(input);
		} else {
			return input;
		}
	}

	/** Same as calling process() for each sample in @a src multiplied by
	 *  @a gain, and adding the result to @a dst. However, if @a prev_gain
	 *  is different from @a gain, interpolates between gains for the
	 *  first 64 samples.
	 *
	 * Implemented using mix_buffers_no_gain() and mix_buffers_with_gain()
	 * from runtime_functions.h. */
	void mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float prev_gain, float gain);

  protected:
	/* If this is false, do_set_pan_position() and do_process() are assumed
	 * to be no-ops and are therefore skipped. Must be set by subclasses. */
	bool _active;

	virtual void do_set_pan_position(float pan_position);
	virtual Sample do_process(Sample input);
	virtual void do_mix_buffers(Sample *dst, const Sample *src, pframes_t nframes, float gain);

  private:
	/* Maximum number of frames to interpolate between gains (used by
	 * mix_buffers(); must be a multiple of 16). */
	static const pframes_t _gain_interp_frames = 64;
};

} // namespace

#endif /* __libardour_pan_distribution_buffer_h__ */
