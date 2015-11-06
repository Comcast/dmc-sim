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
#include "udp-gossip-helper.h"
#include "ns3/udp-gossip.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/dvgp.h"

namespace ns3 {

UdpGossipHelper::UdpGossipHelper (uint16_t port, bool d3_output, 
                                  DmcDataFactory* fact)
{
  m_factory.SetTypeId (UdpGossip::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
  SetAttribute ("D3Output", BooleanValue(d3_output));
  m_dmc_factory = fact;
}

void 
UdpGossipHelper::SetAttribute (
  std::string name, 
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
UdpGossipHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpGossipHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
UdpGossipHelper::Install (NodeContainer nodes) const
{
  ApplicationContainer out;
  for(uint32_t i=0; i < nodes.GetN(); i++) {
    out.Add(InstallPriv(nodes.Get(i)));
  }
  return out;
}

Ptr<Application>
UdpGossipHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<UdpGossip> udp = (Ptr<UdpGossip>)m_factory.Create<UdpGossip>();
  udp->SetDmcData(m_dmc_factory->Create());
  Ptr<Application> app = (Ptr<Application>)udp;
  
  node->AddApplication (app);

  return app;
}

} // namespace ns3
