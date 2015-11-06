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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <time.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DMC");

int
main (int argc, char *argv[])
{
  uint32_t num_nodes = 2;
  uint32_t branch_factor = 3;
  uint32_t secs_to_run = 10;
  bool d3_output = false;

  srand(time(NULL));

  CommandLine cmd;
  cmd.AddValue("numNodes", "number of nodes to create", num_nodes);
  cmd.AddValue("branchFactor", "approximate num connections per node", 
               branch_factor);
  cmd.AddValue("secsToRun", "number of seconds to simulate", secs_to_run);
  cmd.AddValue("d3", "emit d3-friendly JSON", d3_output);
  cmd.Parse (argc, argv);

  if (d3_output) {
    NS_LOG_INFO("D3 var nodes = [];");
    NS_LOG_INFO("D3 var links = [];");
    NS_LOG_INFO("D3 var events = [];");
  }
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpGossipApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("LubyMISProtocol", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (num_nodes);

  bool cxns[num_nodes][num_nodes];
  for(uint32_t i=0; i<num_nodes; i++) {
    for(uint32_t j=0; j<num_nodes; j++) {
      cxns[i][j] = false;
    }
  }

  // make sure we have a connected graph
  uint32_t total_edges = 0;
  for(uint32_t i=1; i<num_nodes; i++) {
    // choose someone that already exists
    uint32_t j = rand() % i;
    cxns[i][j] = true;
    cxns[j][i] = true;
    NS_LOG_DEBUG("initial connectivity: connecting " << i << " to " << j);
    total_edges++;
  }

  uint32_t target_edges = num_nodes * branch_factor / 2;
  uint32_t max_edges = num_nodes * (num_nodes - 1) / 2;
  if (target_edges > max_edges) target_edges = max_edges;
  while(total_edges < target_edges) {
    uint32_t i = rand() % num_nodes;
    uint32_t j = rand() % num_nodes;
    if (i != j && !cxns[i][j]) {
      cxns[i][j] = cxns[j][i] = true;
      NS_LOG_DEBUG("extra connectivity: connecting " << i << " to " << j);
      total_edges++;
    }
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  NetDeviceContainer devices;
  uint32_t num_networks = 1;
  // establish point-to-point links
  for(uint32_t i=0; i<num_nodes; i++) {
    for(uint32_t j=i+1; j<num_nodes; j++) {
      if (cxns[i][j]) {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
        pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

        NodeContainer endpoints;
        endpoints.Add(nodes.Get(i));
        endpoints.Add(nodes.Get(j));
        
        NetDeviceContainer ndc = pointToPoint.Install(endpoints);

        Ipv4AddressHelper address;
        Ipv4Address network((num_networks << 3) | 0x0a000000);

        Ipv4Mask mask(0xfffffff8);

        NS_LOG_INFO("num_networks=" << num_networks <<
                     " network=" << network <<
                     " mask=" << mask);

        address.SetBase(network, mask);

        address.Assign(ndc);
        num_networks++;
      }
    }
  }

  DmcDataFactory* fact = new LubyMISFactory(d3_output);
  UdpGossipHelper gossip (7777, d3_output, fact);
  ApplicationContainer apps;
  apps.Add(gossip.Install(nodes));

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds ((secs_to_run + 1) * 1.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
