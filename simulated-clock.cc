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
#include <cmath>
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "simulated-clock.h"

namespace ns3 {
  
  NS_LOG_COMPONENT_DEFINE("SimulatedClock");

  // Box-Muller Transform
  double SimulatedClock::SampleNormal(double mean, double stddev) {
    NS_LOG_FUNCTION(this);
    double u1 = ((double)rand()) / RAND_MAX;
    double u2 = ((double)rand()) / RAND_MAX;
    double x = sqrt(-2 * log(u1)) * cos(2 * M_PI * u2);
    return (stddev * x + mean);
  }

  SimulatedClock::SimulatedClock(double rate_ratio_mean,
                                 double rate_ratio_stddev,
                                 int offset_mean,
                                 int offset_stddev) {
    NS_LOG_FUNCTION(this);
    m_rate_ratio = SampleNormal(rate_ratio_mean, rate_ratio_stddev);
    m_offset_millis = (int)SampleNormal((double)offset_mean,
                                        (double)offset_stddev);
    m_counter = 0;
    NS_LOG_INFO("m_rate_ratio=" << m_rate_ratio <<
                " m_offset_millis=" << m_offset_millis);
  }

  /* This is based off the ns3 virtual simulator clock, except that we
     have a clock that runs at a different relative rate and starts at some
     initial offset from the simulator clock. Have to do some fudging to
     ensure monotonicity at the beginning of the simulation. */
  Time SimulatedClock::Now() {
    NS_LOG_FUNCTION(this);
    int64_t sim_now_millis = Simulator::Now().GetMilliSeconds();
    int64_t target_millis = ((int64_t)(sim_now_millis * m_rate_ratio)) +
      m_offset_millis;
    if (target_millis < 0 || target_millis < m_counter) {
      target_millis = m_counter++;
    }
    NS_LOG_DEBUG("target_millis=" << target_millis);
    return Time::From(target_millis, Time::MS);
  }

  
}
