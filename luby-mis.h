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
#ifndef _LUBY_H
#define _LUBY_H

#include <map>
#include <vector>
#include <utility>
#include "ns3/ipv4-address.h"
#include "dmc-data.h"

namespace ns3 {

  class LubyPeer {
  public:
    uint32_t degree;
    double value;
    uint32_t next_hop;
    uint32_t dist;
  };

  class LubyLevel {
  public:
    LubyLevel();
    ~LubyLevel();
    uint32_t MaxPeerDistance();

    uint32_t GetMarshalledSize();
    uint8_t* MarshalTo(const uint8_t *buf);
    static LubyLevel* MarshalFrom(const uint8_t *buf);
    void ResetPeers();
    void ResetRep();

    uint32_t level;
    uint32_t rep;
    uint32_t rep_next_hop;
    uint32_t rep_dist;
    double rep_value;
    std::map<uint32_t, LubyPeer*> peers;
  };

  class LubyMIS : public DmcData {
    
  public:
    LubyMIS(bool d3_output);
    virtual ~LubyMIS();
    
    uint32_t GetMarshalledSize();
    void MarshalTo(uint8_t const *buf);
    void MarshalFrom(uint8_t const *buf);
    void LogMemory();
    void SetMyIpv4Address(Ipv4Address me);

  private:
    void AppendIpv4AddressAsString(char *buf, uint32_t addr);
    void ProcessTopologyChanges(uint32_t sender,
                                std::vector<LubyLevel*> msg_levels,
                                uint32_t n);
    void SetMaxLevel(uint32_t level);
    void TryToStartNewLevel();
    void TrimVacatedLeadersAndPeers(uint32_t sender,
                                    std::vector<LubyLevel*> msg_levels);
    void HandleRepElection(uint32_t sender,
                           std::vector<LubyLevel*> msg_levels,
                           uint32_t n);
    void TryToBecomeRep();
    void UpdatePeerValues(uint32_t sender, std::vector<LubyLevel*> msg_levels);
    void RecalculateLevelValues();
    void DumpState(const char *label);
    void DumpMessage(uint32_t sender, std::vector<LubyLevel*> msg_levels);

    uint32_t m_myaddr;
    Ipv4Address m_myip;
    double m_value;
    std::vector<LubyLevel*> m_levels;
    bool m_d3_output;
  };

  class LubyMISFactory : public DmcDataFactory {
    
  public:
    LubyMISFactory(bool d3_output);
    ~LubyMISFactory();

    DmcData* Create();

  private:
    bool m_d3_output;
  };
}


#endif
