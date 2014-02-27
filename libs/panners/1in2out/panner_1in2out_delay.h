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

#ifndef __ardour_panner_1in2out_delay_h__
#define __ardour_panner_1in2out_delay_h__

#include "panner_1in2out.h"

namespace ARDOUR {

class Panner1in2outDelay : public Panner
{
  public:
	Panner1in2outDelay (boost::shared_ptr<Pannable>);
	~Panner1in2outDelay ();

	void set_position (double);
	bool clamp_position (double&);
	std::pair<double, double> position_range () const;

	double position() const;

	ChanCount in() const { return ChanCount (DataType::AUDIO, 1); }
	ChanCount out() const { return ChanCount (DataType::AUDIO, 2); }

	std::set<Evoral::Parameter> what_can_be_automated() const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::string describe_parameter (Evoral::Parameter);
	std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	XMLNode& get_state ();

	void reset ();

	protected:
	float left;
	float right;
	float desired_left;
	float desired_right;

	boost::shared_ptr<PanDistributionBuffer> left_dist_buf;
	boost::shared_ptr<PanDistributionBuffer> right_dist_buf;

	void distribute_one (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
			framepos_t start, framepos_t end, pframes_t nframes,
			pan_t** buffers, uint32_t which);

	void update ();
};

} // namespace

#endif /* __ardour_panner_1in2out_delay_h__ */
