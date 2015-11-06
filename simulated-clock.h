/* 
 * Copyright (C) 2015 Comcast Cable Communications Management, LLC
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _SIMULATED_CLOCK_H
#define _SIMULATED_CLOCK_H

#include "ns3/nstime.h"

namespace ns3 {

  class SimulatedClock {
  public:
    SimulatedClock(double rate_ratio_mean, double rate_ratio_stddev,
                   int offset_mean, int offset_stddev);
    Time Now();
  private:
    double m_rate_ratio;
    int m_offset_millis;
    int64_t m_counter;
    double SampleNormal(double mean, double stddev);
  };

}


#endif
