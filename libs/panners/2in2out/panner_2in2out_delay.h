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

#ifndef __ardour_panner_2in2out_delay_h__
#define __ardour_panner_2in2out_delay_h__

namespace ARDOUR {

class Panner2in2outDelay : public Panner
{
  public:
	Panner2in2outDelay (boost::shared_ptr<Pannable>);
	~Panner2in2outDelay ();

	ChanCount in() const { return ChanCount (DataType::AUDIO, 2); }
	ChanCount out() const { return ChanCount (DataType::AUDIO, 2); }

	bool clamp_position (double&);
	bool clamp_width (double&);

	std::pair<double, double> position_range () const;
	std::pair<double, double> width_range () const;

	void set_position (double);
	void set_width (double);

	double position () const;
	double width () const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	std::set<Evoral::Parameter> what_can_be_automated() const;

	std::string describe_parameter (Evoral::Parameter);
	std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	XMLNode& get_state ();

	void update ();

	void reset ();
	void thaw ();

	protected:
	float left[2];
	float right[2];
	float desired_left[2];
	float desired_right[2];

	/* We need four distribution buffers instead of two because distribute_one()
	 * is called separately for each input. */
	boost::shared_ptr<PanDistributionBuffer> left_dist_buf[2];
	boost::shared_ptr<PanDistributionBuffer> right_dist_buf[2];

	private:
	bool clamp_stereo_pan (double& direction_as_lr_fract, double& width);

	void distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
			framepos_t start, framepos_t end, pframes_t nframes,
			pan_t** buffers, uint32_t which);
};

} // namespace

#endif /* __ardour_panner_2in2out_delay_h__ */
