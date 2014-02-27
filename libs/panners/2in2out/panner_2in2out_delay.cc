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

#include "panner_2in2out_delay.h"

#include "ardour/pannable.h"
#include "ardour/pan_delay_buffer.h"

using namespace std;
using namespace ARDOUR;

static PanPluginDescriptor _descriptor = {
        "Equal Power Stereo with Delay",
        "http://ardour.org/plugin/panner_2in2out_delay",
        "http://ardour.org/plugin/panner_2in2out#ui",
        2, 2, 
        5000,
        Panner2in2outDelay::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

Panner2in2outDelay::Panner2in2outDelay (boost::shared_ptr<Pannable> p)
	: Panner2in2out (p)
{
	Session& session = p->session();

	left_dist_buf[0].reset(new PanDelayBuffer(session));
	left_dist_buf[1].reset(new PanDelayBuffer(session));
	right_dist_buf[0].reset(new PanDelayBuffer(session));
	right_dist_buf[1].reset(new PanDelayBuffer(session));
}

Panner2in2outDelay::~Panner2in2outDelay ()
{
}

Panner*
Panner2in2outDelay::factory (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> /* ignored */)
{
	return new Panner2in2outDelay (p);
}
