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

class Panner1in2outDelay : public Panner1in2out
{
  public:
	Panner1in2outDelay (boost::shared_ptr<Pannable>);
	~Panner1in2outDelay ();

        static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);
};

} // namespace

#endif /* __ardour_panner_1in2out_delay_h__ */
