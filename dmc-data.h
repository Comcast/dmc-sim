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
#ifndef DMC_DATA_H
#define DMC_DATA_H

#include "ns3/ipv4-address.h"

namespace ns3 {
  
  class DmcData {

  public:
    virtual uint32_t GetMarshalledSize() = 0;
    virtual void MarshalTo(uint8_t const *buf) = 0;
    virtual void MarshalFrom(uint8_t const *buf) = 0;
    virtual void LogMemory() = 0;
    virtual void SetMyIpv4Address(Ipv4Address me) = 0;
  };

  class DmcDataFactory {
  public:
    virtual DmcData* Create() = 0;
  };
}

#endif
