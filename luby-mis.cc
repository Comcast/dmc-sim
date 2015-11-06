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
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <math.h>
#include <string.h>
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"
#include "luby-mis.h"

namespace ns3 {
  
  NS_LOG_COMPONENT_DEFINE("LubyMISProtocol");

  LubyLevel::LubyLevel() {
    rep = 0;
    rep_next_hop = 0;
    rep_dist = 0;
    rep_value = 0.0;
  }

  LubyLevel::~LubyLevel() {
    for(std::map<uint32_t, LubyPeer*>::iterator it = peers.begin();
        it != peers.end(); it++) {
      delete it->second;
    }
  }

  uint32_t LubyLevel::MaxPeerDistance() {
    // 3 * 2^(n-1)
    if (level == 0) return 1;
    // 2^(n-1) == (1 << (n-1))
    return 3 * (1 << (level-1));
  }

  uint32_t LubyLevel::GetMarshalledSize() {
    NS_LOG_FUNCTION(this);
    uint32_t per_peer = 4 * sizeof(uint32_t) + sizeof(double);
    uint32_t sz = (sizeof(uint32_t) +  // level
                   sizeof(uint32_t) +      // rep
                   sizeof(uint32_t) +      // rep_dist
                   sizeof(double) +        // rep_value
                   sizeof(uint32_t) +      // num peers
                   peers.size() * per_peer); // peers
    return sz;
  }

  uint8_t* LubyLevel::MarshalTo(const uint8_t *buf) {
    NS_LOG_FUNCTION(this);
    uint8_t *dst = (uint8_t *)buf;
    memcpy(dst, &level, sizeof(level));
    dst += sizeof(level);
    memcpy(dst, &rep, sizeof(rep));
    dst += sizeof(rep);
    memcpy(dst, &rep_dist, sizeof(rep_dist));
    dst += sizeof(rep_dist);
    memcpy(dst, &rep_value, sizeof(rep_value));
    dst += sizeof(rep_value);
    uint32_t num_peers = peers.size();
    memcpy(dst, &num_peers, sizeof(num_peers));
    dst += sizeof(num_peers);
    for(std::map<uint32_t, LubyPeer*>::iterator it = peers.begin();
        it != peers.end(); it++) {
      memcpy(dst, &(it->first), sizeof(it->first));
      dst += sizeof(it->first);
      memcpy(dst, &(it->second->degree), sizeof(it->second->degree));
      dst += sizeof(it->second->degree);
      memcpy(dst, &(it->second->value), sizeof(it->second->value));
      dst += sizeof(it->second->value);
      memcpy(dst, &(it->second->dist), sizeof(it->second->dist));
      dst += sizeof(it->second->dist);
      memcpy(dst, &(it->second->next_hop), sizeof(it->second->next_hop));
      dst += sizeof(it->second->next_hop);
    }
    return dst;
  }

  LubyLevel* LubyLevel::MarshalFrom(const uint8_t *buf) {
    uint8_t *src = (uint8_t *)buf;
    LubyLevel* out = new LubyLevel();
    memcpy(&out->level, src, sizeof(out->level));
    src += sizeof(out->level);
    memcpy(&out->rep, src, sizeof(out->rep));
    src += sizeof(out->rep);
    memcpy(&out->rep_dist, src, sizeof(out->rep_dist));
    src += sizeof(out->rep_dist);
    memcpy(&out->rep_value, src, sizeof(out->rep_value));
    src += sizeof(out->rep_value);
    uint32_t num_peers;
    memcpy(&num_peers, src, sizeof(num_peers));
    src += sizeof(num_peers);
    while(num_peers > 0) {
      uint32_t peer_addr;
      LubyPeer* peer = new LubyPeer();
      memcpy(&peer_addr, src, sizeof(peer_addr));
      src += sizeof(peer_addr);
      memcpy(&peer->degree, src, sizeof(peer->degree));
      src += sizeof(peer->degree);
      memcpy(&peer->value, src, sizeof(peer->value));
      src += sizeof(peer->value);
      memcpy(&peer->dist, src, sizeof(peer->dist));
      src += sizeof(peer->dist);
      memcpy(&peer->next_hop, src, sizeof(peer->next_hop));
      src += sizeof(peer->next_hop);
      out->peers[peer_addr] = peer;
      num_peers--;
    }
    return out;
  }

  void LubyLevel::ResetPeers() {
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("resetting level " << level << " peers");
    for(std::map<uint32_t, LubyPeer*>::iterator it = peers.begin();
        it != peers.end(); it++) {
      delete it->second;
    }
    peers.clear();
  }

  void LubyLevel::ResetRep() {
    NS_LOG_FUNCTION(this);
    rep = 0;
    rep_next_hop = 0;
  }

  LubyMIS::LubyMIS(bool d3_output) {
    m_d3_output = d3_output;
    m_value = ((double)rand()) / RAND_MAX;
    LubyLevel* level0 = new LubyLevel();
    level0->level = 0;
    m_levels.push_back(level0);
  }
  
  LubyMIS::~LubyMIS() {}

  uint32_t LubyMIS::GetMarshalledSize() {
    NS_LOG_FUNCTION(this);
    uint32_t total = (2 * sizeof(uint32_t)); // sender, num_levels
    for(std::vector<LubyLevel*>::iterator it = m_levels.begin();
        it != m_levels.end(); it++) {
      total += (*it)->GetMarshalledSize();
    }
    return total;
  }

  void LubyMIS::AppendIpv4AddressAsString(char *buf, uint32_t a) {
    sprintf(buf + strlen(buf), "%d.", a >> 24);
    sprintf(buf + strlen(buf), "%d.", (a >> 16) & 0xff);
    sprintf(buf + strlen(buf), "%d.", (a >> 8) & 0xff);
    sprintf(buf + strlen(buf), "%d", a & 0xff);
  }

  void LubyMIS::MarshalTo(uint8_t const *buf) {
    NS_LOG_FUNCTION(this);
    uint8_t *dst = (uint8_t *)buf;
    memcpy(dst, &(m_levels.at(0)->rep), sizeof(uint32_t));
    dst += sizeof(uint32_t);
    uint32_t num_levels = m_levels.size();
    memcpy(dst, &num_levels, sizeof(num_levels));
    dst += sizeof(num_levels);
    for(std::vector<LubyLevel*>::iterator it = m_levels.begin();
        it != m_levels.end(); it++) {
      dst = (*it)->MarshalTo(dst);
    }
  }

  void LubyMIS::DumpState(const char *label) {
    NS_LOG_FUNCTION(this);
    char dbuf[4096];
    sprintf(dbuf, "%s { m_myaddr: ", label);
    AppendIpv4AddressAsString(dbuf, m_myaddr);
    sprintf(dbuf + strlen(dbuf), ", levels: [");
    for(std::vector<LubyLevel*>::iterator it = m_levels.begin();
        it != m_levels.end(); it++) {
      if (it != m_levels.begin()) {
        sprintf(dbuf + strlen(dbuf), ", ");
      }
      sprintf(dbuf + strlen(dbuf), "{ level: %d, ", (*it)->level);
      sprintf(dbuf + strlen(dbuf), "rep: ");
      AppendIpv4AddressAsString(dbuf, (*it)->rep);
      sprintf(dbuf + strlen(dbuf), ", peers: [");
      for(std::map<uint32_t, LubyPeer*>::iterator it2 = (*it)->peers.begin();
          it2 != (*it)->peers.end(); it2++) {
        if (it2 != (*it)->peers.begin()) {
          sprintf(dbuf + strlen(dbuf), ", ");
        }
        sprintf(dbuf + strlen(dbuf), "{ peer: ");
        AppendIpv4AddressAsString(dbuf, it2->first);
        sprintf(dbuf + strlen(dbuf), ", degree: %d }", it2->second->degree);
      }
      sprintf(dbuf + strlen(dbuf), "]}");
    }
    sprintf(dbuf + strlen(dbuf), "]}");
    NS_LOG_INFO(dbuf);
  }

  void LubyMIS::DumpMessage(uint32_t sender, std::vector<LubyLevel*> msg_levels) {
    NS_LOG_FUNCTION(this);
    char dbuf[4096];
    sprintf(dbuf, "RECV_MSG { sender: ");
    AppendIpv4AddressAsString(dbuf, sender);
    sprintf(dbuf + strlen(dbuf), ", levels: [");
    for(std::vector<LubyLevel*>::iterator it = msg_levels.begin();
        it != msg_levels.end(); it++) {
      if (it != msg_levels.begin()) {
        sprintf(dbuf + strlen(dbuf), ", ");
      }
      sprintf(dbuf + strlen(dbuf), "{ level: %d, ", (*it)->level);
      sprintf(dbuf + strlen(dbuf), "rep: ");
      AppendIpv4AddressAsString(dbuf, (*it)->rep);
      sprintf(dbuf + strlen(dbuf), ", peers: [");
      for(std::map<uint32_t, LubyPeer*>::iterator it2 = (*it)->peers.begin();
          it2 != (*it)->peers.end(); it2++) {
        if (it2 != (*it)->peers.begin()) {
          sprintf(dbuf + strlen(dbuf), ", ");
        }
        sprintf(dbuf + strlen(dbuf), "{ peer: ");
        AppendIpv4AddressAsString(dbuf, it2->first);
        sprintf(dbuf + strlen(dbuf), ", degree: %d }", it2->second->degree);
      }
      sprintf(dbuf + strlen(dbuf), "]}");
    }
    sprintf(dbuf + strlen(dbuf), "]}");
    NS_LOG_INFO(dbuf);
  }

  void LubyMIS::MarshalFrom(uint8_t const *buf) {
    NS_LOG_FUNCTION(this);

    //DumpState("PRE");

    uint8_t *src = (uint8_t *)buf;
    uint32_t sender;
    memcpy(&sender, src, sizeof(sender));
    src += sizeof(sender);
    uint32_t num_levels;
    memcpy(&num_levels, src, sizeof(num_levels));
    src += sizeof(num_levels);

    std::vector<LubyLevel*> msg_levels;
    while(num_levels > 0) {
      LubyLevel* lvl = LubyLevel::MarshalFrom(src);
      src += lvl->GetMarshalledSize();
      msg_levels.push_back(lvl);
      num_levels--;
    }

    //DumpMessage(sender, msg_levels);
    
    for(uint32_t n = 0; n < msg_levels.size() && n < m_levels.size(); n++) {
      ProcessTopologyChanges(sender, msg_levels, n);
      TryToStartNewLevel();
      if (n < msg_levels.size() && n < m_levels.size()) {
        HandleRepElection(sender, msg_levels, n);
      }
    }

    TrimVacatedLeadersAndPeers(sender, msg_levels);

    UpdatePeerValues(sender, msg_levels);
    RecalculateLevelValues();
    TryToBecomeRep();

    //DumpState("POST");

    for(std::vector<LubyLevel *>::iterator it = msg_levels.begin();
        it != msg_levels.end(); it++) {
      delete (*it);
    }
  }

  void LubyMIS::ProcessTopologyChanges(uint32_t sender,
                                       std::vector<LubyLevel*> msg_levels,
                                       uint32_t n) {
    Ipv4Address a(sender);
    Ipv4Address a2;

    NS_LOG_FUNCTION(this << n);
    if (n == 0) {
      LubyLevel* lev0 = m_levels.at(0);
      if (lev0->peers.count(sender) == 0) {
        NS_LOG_DEBUG("found new level 0 peer " << a);
        LubyPeer* peer = new LubyPeer();
        peer->degree = msg_levels.at(0)->peers.size();
        peer->next_hop = sender;
        peer->dist = 1;
        lev0->peers[sender] = peer;
        SetMaxLevel(0);
      } else if (lev0->peers[sender]->degree !=
                 msg_levels.at(0)->peers.size()) {
        a2.Set(sender);
        NS_LOG_DEBUG("detected change in degree for level " << n <<
                     " peer " << a2 << ": " << lev0->peers[sender]->degree <<
                     "->" << msg_levels.at(n)->peers.size());
        lev0->peers[sender]->degree = msg_levels.at(0)->peers.size();
        SetMaxLevel(0);
      }
      return;
    }
    
    if (m_levels.at(n)->rep == 0) {
      // if I don't have a level-n rep then I don't know who my peers are
      // anyway; they also should have been cleared out if I ever had any
      NS_ASSERT(m_levels.at(n)->peers.size() == 0);
      return;
    }
    
    if (msg_levels.at(n)->rep == 0) {
      // remove any peers where the sender is the next hop
      std::vector<uint32_t> peers_to_remove;
      std::map<uint32_t, LubyPeer*>::iterator it;
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        if (it->second->next_hop == sender) {
          peers_to_remove.push_back(it->first);
        }
      }
      for(std::vector<uint32_t>::iterator peer = peers_to_remove.begin();
          peer != peers_to_remove.end(); peer++) {
        a2.Set(*peer);
        delete m_levels.at(n)->peers[*peer];
        m_levels.at(n)->peers[*peer] = NULL;
        m_levels.at(n)->peers.erase(m_levels.at(n)->peers.find(*peer));
        SetMaxLevel(n);
        NS_LOG_DEBUG("removing level-" << n << " peer " << a2 << 
                     " b/c it was learned via sender " << a << " who no" <<
                     " longer has a level-" << n << " rep");
                     
      }
      return;
    }

    if (m_levels.at(n)->rep == msg_levels.at(n)->rep) {
      // this is from a member of my level-n group; level n peers should be
      // merged.

      // first, remove any level n peers whose next hop is the sender but
      // where the sender is no longer advertising that peer; also look for
      // peers whose degree has changed.
      std::vector<uint32_t> peers_to_remove;
      std::map<uint32_t, LubyPeer*>::iterator it;
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        if (it->first == sender) {
          // sender is in my level-n group; it can't be a peer!
          NS_LOG_DEBUG("sender " << a << " is in my level " << n <<
                       " group but is listed as a level " << n << " peer");
          peers_to_remove.push_back(it->first);
        } else if (it->second->next_hop == sender) {
          a2.Set(it->first);
          if (msg_levels.at(n)->peers.count(it->first) == 0) {
            NS_LOG_DEBUG("sender " << a << " no longer advertising level "
                         << n << " peer " << a2);
            peers_to_remove.push_back(it->first);
          } else if (it->second->next_hop == m_myaddr) {
            NS_LOG_DEBUG("sender " << a << " learned route to peer " << a2
                         << " from me; dropping (split horizon)");
            peers_to_remove.push_back(it->first);
          } else if (it->second->dist == 1) {
            // if distance was 1 then the sender wasn't in my level-n
            // group when I learned of this peer
            NS_LOG_DEBUG("sender " << a << " used to be part of level "
                         << n << " peer " << a2 << " but isn't anymore");
            peers_to_remove.push_back(it->first);
          } else if (


              // watch for routing loops
              (msg_levels.at(n)->peers[it->first]->dist + 1 >
               m_levels.at(n)->MaxPeerDistance())) {
            NS_LOG_DEBUG("routing loop to level-" << n << " peer " << a2
                         << " detected");
            peers_to_remove.push_back(it->first);
          } else if (msg_levels.at(n)->peers[it->first]->degree != it->second->degree) {
            a2.Set(it->first);
            NS_LOG_DEBUG("detected change in degree for level " << n <<
                         " peer " << a2 << ": " << it->second->degree <<
                         "->" << msg_levels.at(n)->peers[it->first]->degree);
            it->second->degree = msg_levels.at(n)->peers[it->first]->degree;
            SetMaxLevel(n);
          }
          // adjust route distance
          m_levels.at(n)->peers[it->first]->dist = it->second->dist + 1;
        }
      }
      for(std::vector<uint32_t>::iterator peer = peers_to_remove.begin();
          peer != peers_to_remove.end(); peer++) {
        a2.Set(*peer);
        delete m_levels.at(n)->peers[*peer];
        m_levels.at(n)->peers[*peer] = NULL;
        m_levels.at(n)->peers.erase(m_levels.at(n)->peers.find(*peer));
        SetMaxLevel(n);
        NS_LOG_DEBUG("removing level " << n << " peer " << a2 << " (#1)");
      }

      // see if the sender has a lower-distance route to a peer than my
      // current next hop
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        if (msg_levels.at(n)->peers.count(it->first) > 0) {
          LubyPeer* sender_peer = msg_levels.at(n)->peers[it->first];
          if (sender_peer->dist + 1 < it->second->dist) {
            // better route
            it->second->next_hop = sender;
            it->second->dist = sender_peer->dist + 1;
            if (sender_peer->degree != it->second->degree) {
              a2.Set(it->first);
              NS_LOG_DEBUG("detected change in degree for level " << n <<
                           " peer " << a2 << ": " << it->second->degree <<
                           "->" << sender_peer->degree);
              it->second->degree = sender_peer->degree;
              SetMaxLevel(n);
            }
          }
        }
      }
      
      // next, add any new level-n peers with the sender as the next hop
      for(it = msg_levels.at(n)->peers.begin();
          it != msg_levels.at(n)->peers.end(); it++) {
        if (it->first != m_myaddr && // I can't be my own peer!
            m_levels.at(n)->peers.count(it->first) == 0 &&
            it->second->next_hop != m_myaddr && // split horizon
            it->second->dist + 1 <= msg_levels.at(n)->MaxPeerDistance()) {
          LubyPeer* p = new LubyPeer();
          p->degree = it->second->degree;
          p->next_hop = sender;
          p->dist = it->second->dist + 1;
          m_levels.at(n)->peers[it->first] = p;
          SetMaxLevel(n);
          a2.Set(it->first);
          NS_LOG_DEBUG("adding new level " << n << " peer " << a2 << 
                       " at distance " << p->dist);
        }
      }
    } else {
      // this is from a member of a level-n peer group; 
      
      // remove any level-n
      // peers where the sender is the next hop but the sender's rep is
      // different; also check if the peer's degree changed
      std::vector<uint32_t> peers_to_remove;
      std::map<uint32_t, LubyPeer*>::iterator it;
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        if (it->first == msg_levels.at(n)->rep &&
            it->second->dist > 1) {
          peers_to_remove.push_back(it->first);
        } else if (it->second->next_hop == sender) {
          if (it->first != msg_levels.at(n)->rep) {
            peers_to_remove.push_back(it->first);
          } else if (it->second->degree != msg_levels.at(n)->peers.size()) {
            a2.Set(it->first);
            NS_LOG_DEBUG("detected degree change for level " << n <<
                         " peer " << a2 << ": " << it->second->degree <<
                         "->" << msg_levels.at(n)->peers.size());
            if (msg_levels.at(n)->peers.size() > 0) {
              it->second->degree = msg_levels.at(n)->peers.size();
              SetMaxLevel(n);
            } else {
              peers_to_remove.push_back(it->first);
            }
          }
        }
      }
      for(std::vector<uint32_t>::iterator peer = peers_to_remove.begin();
          peer != peers_to_remove.end(); peer++) {
        a2.Set(*peer);
        NS_LOG_DEBUG("removing level " << n << " peer " << a2 << " (#2)");
        delete m_levels.at(n)->peers[*peer];
        m_levels.at(n)->peers[*peer] = NULL;
        m_levels.at(n)->peers.erase(m_levels.at(n)->peers.find(*peer));
        SetMaxLevel(n);
      }

      // then if the sender's rep is not present we should add
      // it as a peer.
      if (msg_levels.at(n)->rep != 0 &&
          m_levels.at(n)->peers.count(msg_levels.at(n)->rep) == 0) {
        if (msg_levels.at(n)->rep != m_myaddr) {
          LubyPeer* p = new LubyPeer();
          p->degree = msg_levels.at(n)->peers.size();
          p->next_hop = sender;
          p->dist = 1;
          m_levels.at(n)->peers[msg_levels.at(n)->rep] = p;
          SetMaxLevel(n);
          a2.Set(msg_levels.at(n)->rep);
          NS_LOG_DEBUG("adding new level " << n << " peer " << a2);
        }
      }
      
      // sender's rep should be a peer of distance 1
      if (msg_levels.at(n)->rep != 0 &&
          m_levels.at(n)->peers.count(msg_levels.at(n)->rep) > 0) {
        m_levels.at(n)->peers[msg_levels.at(n)->rep]->dist = 1;
      }
    }

    if (m_levels.at(n)->peers.size() == 0 &&
        m_levels.size() - 1 > n) {
      NS_LOG_DEBUG("don't have any level-" << n <<
                   " peers; should not be running at a higher level");
      SetMaxLevel(n);
    }
    if (m_levels.at(n)->rep == m_myaddr &&
        m_levels.at(n-1)->peers.size() == 0) {
      NS_LOG_DEBUG("I'm a level " << n << " rep but have no level " <<
                   (n-1) << " peers; dropping level");
      SetMaxLevel(n-1);
    }
  }


  void LubyMIS::SetMaxLevel(uint32_t level) {
    NS_LOG_FUNCTION(this);
    if (m_levels.size() - 1 <= level) return;
    if (m_d3_output) {
      NS_LOG_INFO("D3 events.push({type:\"maxlevel\",node:\"" <<
                  m_myip << "\",level:" << level << ",time:" <<
                  Simulator::Now().GetSeconds() << "});");
    } else {
      NS_LOG_DEBUG("setting max level to " << level);
    }
    while(m_levels.size() > level + 1) {
      LubyLevel *lvl = m_levels.at(m_levels.size() - 1);
      delete lvl;
      m_levels.erase(m_levels.end() - 1);
    }
  }

  void LubyMIS::TryToStartNewLevel() {
    NS_LOG_FUNCTION(this);
    LubyLevel* max_level = m_levels.at(m_levels.size() - 1);
    if (max_level->rep != 0 && max_level->peers.size() > 0) {
      LubyLevel* new_level = new LubyLevel();
      new_level->level = max_level->level + 1;
      m_levels.push_back(new_level);
      NS_LOG_INFO("beginning protocol level " << new_level->level);
    }
  }

  void LubyMIS::HandleRepElection(uint32_t sender,
                                  std::vector<LubyLevel*> msg_levels,
                                  uint32_t n) {
    Ipv4Address a;
    NS_LOG_FUNCTION(this << n);
    if (n == 0) return;         // I am always my own level 0 rep

    // correct for no-longer valid reps; my level-n rep either has to be
    // my level n-1 rep or one of my level n-1 peers
    if ((m_levels.at(n)->rep != 0 &&
         m_levels.at(n)->rep != m_levels.at(n-1)->rep &&
         m_levels.at(n-1)->peers.count(m_levels.at(n)->rep) == 0)
        ||
        (m_levels.at(n)->rep_next_hop == sender &&
         m_levels.at(n)->rep != msg_levels.at(n)->rep)
        ) {
      a.Set(m_levels.at(n)->rep);
      m_levels.at(n)->rep = m_levels.at(n)->rep_next_hop = m_levels.at(n)->rep_dist = 0;
      SetMaxLevel(n);
      m_levels.at(n)->ResetPeers();
      if (m_d3_output) {
        NS_LOG_INFO("D3 events.push({type:\"unelect\",node:\"" <<
                    m_myip << "\",level:" << n << ",time:" <<
                    Simulator::Now().GetSeconds() << "});");
      } else {
        NS_LOG_INFO("REP: deselected level " << n << 
                    " rep " << a << " b/c it is neither my level " <<
                    (n-1) << " rep nor one of my level " << (n-1) << " peers");
      }
    }

    if (m_levels.at(n)->rep == sender &&
        msg_levels.at(n)->rep != sender) {
        m_levels.at(n)->rep = m_levels.at(n)->rep_next_hop = m_levels.at(n)->rep_dist = 0;
        SetMaxLevel(n);
        m_levels.at(n)->ResetPeers();
        a.Set(sender);
        if (m_d3_output) {
          NS_LOG_INFO("D3 events.push({type:\"unelect\",node:\"" <<
                      m_myip << "\",level:" << n << ",time:" <<
                      Simulator::Now().GetSeconds() << "});");
        } else {
          NS_LOG_INFO("REP: deselected level " << n << 
                      " rep b/c rep_next_hop " << a <<
                      " is no longer advertising a rep");
        }
    }

    if (msg_levels.at(n)->rep == 0) {
      if (m_levels.at(n)->rep_next_hop == sender) {
        // unelect level-n rep
        m_levels.at(n)->rep = m_levels.at(n)->rep_next_hop = m_levels.at(n)->rep_dist = 0;
        SetMaxLevel(n);
        m_levels.at(n)->ResetPeers();
        a.Set(sender);
        if (m_d3_output) {
          NS_LOG_INFO("D3 events.push({type:\"unelect\",node:\"" <<
                      m_myip << "\",level:" << n << ",time:" <<
                      Simulator::Now().GetSeconds() << "});");
        } else {
          NS_LOG_INFO("REP: deselected level " << n << 
                      " rep b/c rep_next_hop " << a <<
                      " is no longer advertising a rep");
        }
      } else {
        // nothing to do here; ignore it
      }
    } else {
      // sender has a level-n rep that is a possibility for me because
      // it is either my level n-1 rep or is one of my n-1 peers
      if ((msg_levels.at(n)->rep == m_levels.at(n-1)->rep ||
           m_levels.at(n-1)->peers.count(msg_levels.at(n)->rep) > 0) &&

          // this would be a rep change
          msg_levels.at(n)->rep != m_levels.at(n)->rep &&

          // this is a valid route
          // msg_levels.at(n)->rep_dist + 1 < msg_levels.at(n)->MaxPeerDistance() &&

          (// I'm only sixteen, I don't have a rep yet.
           (m_levels.at(n)->rep == 0)
           ||

           // My current level n rep is one of my level n-1 peers, and
           // the sender is part of my level n-1 group and that rep has
           // a higher degree than my current one
           (m_levels.at(n-1)->peers.count(m_levels.at(n)->rep) > 0 &&
            msg_levels.at(n)->rep == m_levels.at(n-1)->rep &&
            (m_levels.at(n-1)->peers.size() >
             m_levels.at(n-1)->peers[m_levels.at(n)->rep]->degree))
           ||

           // My current level n rep is one of my level n-1 peers, and
           // the sender is part of my level n-1 group and that rep has
           // the same degree but lower address than my current one
           (m_levels.at(n-1)->peers.count(m_levels.at(n)->rep) > 0 &&
            msg_levels.at(n)->rep == m_levels.at(n-1)->rep &&
            (m_levels.at(n-1)->peers.size() ==
             m_levels.at(n-1)->peers[m_levels.at(n)->rep]->degree) &&
            msg_levels.at(n)->rep < m_levels.at(n)->rep)
           ||

           // My current level n rep is also my level n-1 rep, but the
           // sender is part of a level n-1 peer with higher degree
           (m_levels.at(n)->rep == m_levels.at(n-1)->rep &&
            m_levels.at(n-1)->rep != msg_levels.at(n-1)->rep &&
            m_levels.at(n-1)->peers.count(msg_levels.at(n)->rep) != 0 &&
            (m_levels.at(n-1)->peers[msg_levels.at(n)->rep]->degree >
             m_levels.at(n-1)->peers.size()))
           ||
           // My current level n rep is also my level n-1 rep, but the
           // sender is part of a level n-1 peer with the same degree but
           // a lower address
           (m_levels.at(n)->rep == m_levels.at(n-1)->rep &&
            m_levels.at(n-1)->rep != msg_levels.at(n-1)->rep &&
            m_levels.at(n-1)->peers.count(msg_levels.at(n)->rep) != 0 &&
            (m_levels.at(n-1)->peers[msg_levels.at(n)->rep]->degree ==
             m_levels.at(n-1)->peers.size()) &&
            msg_levels.at(n)->rep < m_levels.at(n)->rep)
           ||
           // Sender is part of a level n-1 peer with a higher degree than
           // the level n-1 peer that is currently my rep
           (m_levels.at(n-1)->rep != msg_levels.at(n-1)->rep &&
            m_levels.at(n)->rep != m_levels.at(n-1)->rep &&
            m_levels.at(n-1)->peers.count(msg_levels.at(n)->rep) != 0 &&
            (m_levels.at(n-1)->peers[msg_levels.at(n)->rep]->degree >
             m_levels.at(n-1)->peers[m_levels.at(n)->rep]->degree))
           ||
           // Sender is part of a level n-1 peer with the same degree as
           // the level n-1 peer that is currently my rep but has a lower
           // address
           (m_levels.at(n-1)->rep != msg_levels.at(n-1)->rep &&
            m_levels.at(n)->rep != m_levels.at(n-1)->rep &&
            m_levels.at(n-1)->peers.count(msg_levels.at(n)->rep) != 0 &&
            (m_levels.at(n-1)->peers[msg_levels.at(n)->rep]->degree ==
             m_levels.at(n-1)->peers[m_levels.at(n)->rep]->degree) &&
            msg_levels.at(n)->rep < m_levels.at(n)->rep))
          ) {
        // take sender's
        m_levels.at(n)->rep = msg_levels.at(n)->rep;
        m_levels.at(n)->rep_next_hop = sender;
        m_levels.at(n)->rep_dist = msg_levels.at(n)->rep_dist + 1;
        SetMaxLevel(n);
        m_levels.at(n)->ResetPeers();
        a.Set(m_levels.at(n)->rep);
        if (m_d3_output) {
          NS_LOG_INFO("D3 events.push({type:\"elect\",node:\"" << m_myip <<
                      "\",level:" << n << ",rep:\"" << a <<
                      "\",time:" << Simulator::Now().GetSeconds() <<
                      "});");
        } else {
          NS_LOG_INFO("REP: selected new level " << n << " rep " << a);
        }
      } else {
        // ignore
      }
    }

    // if sender has a shorter path to my rep, adopt its path
    if (m_levels.at(n)->rep != 0 &&
        m_levels.at(n)->rep == msg_levels.at(n)->rep &&
        msg_levels.at(n)->rep_dist + 1 < m_levels.at(n)->rep_dist) {
      m_levels.at(n)->rep_next_hop = sender;
      m_levels.at(n)->rep_dist = msg_levels.at(n)->rep_dist + 1;
    }

  }

  void LubyMIS::TrimVacatedLeadersAndPeers(uint32_t sender,
                                           std::vector<LubyLevel*> msg_levels) {
    NS_LOG_FUNCTION(this);
    if (msg_levels.size() >= m_levels.size()) return;
    for(uint32_t n = msg_levels.size(); n < m_levels.size(); n++) {
      if (m_levels.at(n)->rep != 0 &&
          m_levels.at(n)->rep_next_hop == sender) {
        m_levels.at(n)->rep = m_levels.at(n)->rep_next_hop = m_levels.at(n)->rep_dist = 0;
        SetMaxLevel(n);
        if (m_d3_output) {
          NS_LOG_INFO("D3 events.push({type:\"unelect\",node:\"" <<
                      m_myip << "\",level:" << n << ",time:" <<
                      Simulator::Now().GetSeconds() << "});");
        } else {
          NS_LOG_INFO("REP: #7: deselected level " << n << 
                      " rep (next hop not on this level anymore)");
        }
        m_levels.at(n)->ResetPeers();
      } else {
        std::map<uint32_t, LubyPeer*>::iterator it;
        std::vector<uint32_t> peers_to_remove;
        for(it = m_levels.at(n)->peers.begin();
            it != m_levels.at(n)->peers.end(); it++) {
          if (it->second->next_hop == sender) {
            NS_LOG_INFO("level " << n << " peer next hop no longer at this level");
            peers_to_remove.push_back(it->first);
          }
        }
        for(std::vector<uint32_t>::iterator peer = peers_to_remove.begin();
            peer != peers_to_remove.end(); peer++) {
          delete m_levels.at(n)->peers[*peer];
          m_levels.at(n)->peers[*peer] = NULL;
          m_levels.at(n)->peers.erase(m_levels.at(n)->peers.find(*peer));
          SetMaxLevel(n);
        }
      }
    }
  }

  void LubyMIS::TryToBecomeRep() {
    Ipv4Address a;
    NS_LOG_FUNCTION(this);
    LubyLevel* max_level = m_levels.at(m_levels.size() - 1);
    if (max_level->rep == 0 &&
        m_levels.at(max_level->level - 1)->rep == m_myaddr &&
        m_levels.at(max_level->level - 1)->peers.size() > 0) {
      // I am a level N-1 rep, so might be able to become level N rep
      double roll = ((double)rand()) / RAND_MAX;
      if (roll < (1.0 / (2 * m_levels.at(max_level->level - 1)->peers.size()))) {
      // if (1) {
        max_level->rep = m_myaddr;
        max_level->rep_next_hop = m_myaddr;
        max_level->rep_dist = 0;
        a.Set(m_myaddr);
        if (m_d3_output) {
          NS_LOG_INFO("D3 events.push({type:\"elect\",node:\"" <<
                      m_myip << "\",level:" << max_level->level <<
                      ",rep:\"" << a << "\",time:" <<
                      Simulator::Now().GetSeconds() << "});");
        } else {
          NS_LOG_INFO("REP: selecting self(" << a << ") as level " <<
                      max_level->level << " rep");
        }
        max_level->ResetPeers();
      }
    }
  }

  void LubyMIS::UpdatePeerValues(uint32_t sender,
                                 std::vector<LubyLevel*> msg_levels) {
    NS_LOG_FUNCTION(this);
    for(uint32_t n = 0; n < m_levels.size() && n < msg_levels.size(); n++) {
      if (m_levels.at(n)->rep == msg_levels.at(n)->rep &&
          m_levels.at(n)->rep_next_hop == sender) {
        m_levels.at(n)->rep_value = msg_levels.at(n)->rep_value;
      }
      std::map<uint32_t, LubyPeer*>::iterator it;
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        if (it->second->next_hop == sender) {
          if (m_levels.at(n)->rep == msg_levels.at(n)->rep) {
            it->second->value = msg_levels.at(n)->peers[it->first]->value;
          } else {
            it->second->value = msg_levels.at(n)->rep_value;
          }
        }
      }
    }
  }

  void LubyMIS::RecalculateLevelValues() {
    NS_LOG_FUNCTION(this);
    for(uint32_t n = 1; n < m_levels.size(); n++) {
      if (m_levels.at(n)->rep == m_myaddr) {
        // I'm the level-n rep, so I should calculate the median value
        // for this level-n group: median of my level-n-1 group's value
        // and the values of my level-n-1 peer groups
        std::vector<double> values;
        values.push_back(m_levels.at(n-1)->rep_value);
        std::map<uint32_t, LubyPeer*>::iterator it;
        for(it = m_levels.at(n-1)->peers.begin();
            it != m_levels.at(n-1)->peers.end(); it++) {
          values.push_back(it->second->value);
        }
        std::sort(values.begin(), values.end());
        if (values.size() % 2 == 1) {
          m_levels.at(n)->rep_value = values.at(values.size() / 2);
        } else {
          m_levels.at(n)->rep_value = 
            (values.at(values.size() / 2 - 1) +
             values.at(values.size() / 2)) / 2.0;
        }
        values.clear();
      }
    }
  }

  void LubyMIS::LogMemory() {
    NS_LOG_FUNCTION(this);
    Ipv4Address me(m_myaddr);
    Ipv4Address p;
    Ipv4Address hop;
    NS_LOG_INFO("memory: msg_sz=" << GetMarshalledSize() <<
                " m_myaddr=" << me <<
                " m_value=" << m_value);
    for(uint32_t n = 0; n < m_levels.size(); n++) {
      Ipv4Address a(m_levels.at(n)->rep);
      NS_LOG_INFO("memory: level=" << n <<
                  " rep=" << a <<
                  " rep_value=" << m_levels.at(n)->rep_value <<
                  " #peers=" << m_levels.at(n)->peers.size());
      uint32_t i = 0;
      std::map<uint32_t, LubyPeer*>::iterator it;
      for(it = m_levels.at(n)->peers.begin();
          it != m_levels.at(n)->peers.end(); it++) {
        p.Set(it->first);
        hop.Set(it->second->next_hop);
        NS_LOG_INFO("  peer " << i << ": " << p << 
                    " (distance " << it->second->dist << " via " << hop
                    << ")");
        i++;
      }
    }
  }

  void LubyMIS::SetMyIpv4Address(Ipv4Address me) {
    NS_LOG_FUNCTION(this);
    uint32_t addr = me.Get();
    m_myaddr = addr;
    m_myip.Set(m_myaddr);
    m_levels.at(0)->rep = addr;
    m_levels.at(0)->rep_value = m_value;
  }

  LubyMISFactory::LubyMISFactory(bool d3_output) {
    m_d3_output = d3_output;
  }
  LubyMISFactory::~LubyMISFactory() {}

  DmcData* LubyMISFactory::Create() {
    return new LubyMIS(m_d3_output);
  }
}
