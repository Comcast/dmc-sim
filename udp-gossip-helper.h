/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
#ifndef UDP_GOSSIP_HELPER_H
#define UDP_GOSSIP_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/dmc-data.h"

namespace ns3 {

/**
 * \ingroup udpgossip
 * \brief Gossip protocols
 */
class UdpGossipHelper
{
public:
  /**
   * Create UdpGossipHelper which will make life easier for people trying
   * to set up simulations with UDP gossip protocols.
   *
   * \param port The port the server will wait on for incoming packets
   * \param d3_output Whether log messages should be emitted in d3-friendly
   *        JSON
   */
  UdpGossipHelper (uint16_t port, bool d3_output, DmcDataFactory* fact);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Create a UdpGossipApplication on the specified Node.
   *
   * \param node The node on which to create the Application.  The node is
   *             specified by a Ptr<Node>.
   *
   * \returns An ApplicationContainer holding the Application created,
   */
  ApplicationContainer Install (Ptr<Node> node) const;

  /**
   * Create a UdpGossipApplication on specified node
   *
   * \param nodeName The node on which to create the application.  The node
   *                 is specified by a node name previously registered with
   *                 the Object Name Service.
   *
   * \returns An ApplicationContainer holding the Application created.
   */
  ApplicationContainer Install (std::string nodeName) const;

  ApplicationContainer Install (NodeContainer nodes) const;

private:
  /**
   * Install an ns3::UdpGossip on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an UdpGossip will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;

  ObjectFactory m_factory; //!< Object factory.

  DmcDataFactory* m_dmc_factory;
};

} // namespace ns3

#endif /* UDP_GOSSIP_HELPER_H */
