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
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "udp-gossip.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpGossipApplication");

NS_OBJECT_ENSURE_REGISTERED (UdpGossip);

TypeId
UdpGossip::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpGossip")
    .SetParent<Application> ()
    .AddConstructor<UdpGossip> ()
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&UdpGossip::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("Port", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&UdpGossip::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("D3Output",
                   "Whether to emit d3.js-friendly JSON log messages",
                   BooleanValue (false),
                   MakeBooleanAccessor (&UdpGossip::m_d3_output),
                   MakeBooleanChecker())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&UdpGossip::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

UdpGossip::UdpGossip ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_sendEvent = EventId();
}

UdpGossip::~UdpGossip()
{
  NS_LOG_FUNCTION (this);
  uint32_t i;
  for(i=0; i < m_num_peers; i++) {
    m_send_sockets[i] = 0;
  }
  delete [] m_send_sockets;
  delete [] m_peer_addresses;
  m_recv_socket = 0;
}

void
UdpGossip::SetPort (uint16_t port)
{
  NS_LOG_FUNCTION (this << port);
  m_port = port;
}

void
UdpGossip::SetDmcData(DmcData *dd) {
  NS_LOG_FUNCTION (this);
  m_dmc_data = dd;
}

void
UdpGossip::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

Ipv4Address
UdpGossip::GetSomeAddrOf(Ptr<Node> node) {
  NS_LOG_FUNCTION (this);
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  for(uint32_t iface = 0; iface < ipv4->GetNInterfaces(); iface++) {
    for(uint32_t addr_idx = 0; addr_idx < ipv4->GetNAddresses(iface);
        addr_idx++) {
      Ipv4InterfaceAddress iaddr = ipv4->GetAddress(iface, addr_idx);
      Ipv4Address addr = iaddr.GetLocal();
      if (addr == Ipv4Address::GetLoopback()) continue;
      NS_LOG_DEBUG("Peer " << node->GetId() << " has address " << addr);
      return addr;
    }
  }
  NS_ASSERT_MSG(false, "could not find address of node" << node->GetId());
  return Ipv4Address::GetZero();
}

bool
UdpGossip::IsLoopbackInterface(uint32_t iface) {
  NS_LOG_FUNCTION(this);
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  for(uint32_t i = 0; i < ipv4->GetNAddresses(iface); i++) {
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress(iface, i);
    if (iaddr.GetLocal() == Ipv4Address::GetLoopback()) return true;
  }
  return false;
}

uint32_t
UdpGossip::GetNumPeers() {
  NS_LOG_FUNCTION(this);
  uint32_t out = 0;
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  for(uint32_t iface = 0; iface < ipv4->GetNInterfaces(); iface++) {
    if (!IsLoopbackInterface(iface)) out++;
  }
  return out;
}

Ipv4Address
UdpGossip::GetPeerAddr(uint32_t iface) {
  NS_LOG_FUNCTION(this);
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ptr<NetDevice> dev = ipv4->GetNetDevice(iface);
  Ptr<Channel> chan = dev->GetChannel();
  Ptr<NetDevice> otherDev;
  for(uint32_t i=0; i < chan->GetNDevices(); i++) {
    otherDev = chan->GetDevice(i);
    if (otherDev->GetAddress() != dev->GetAddress()) break;
  }
  Ptr<Ipv4> otherIpv4 = otherDev->GetNode()->GetObject<Ipv4>();
  uint32_t otherIface = otherIpv4->GetInterfaceForDevice(otherDev);
  Ipv4InterfaceAddress iaddr = otherIpv4->GetAddress(otherIface, 0);
  return iaddr.GetLocal();
}

Ptr<Node>
UdpGossip::GetPeerNode(uint32_t iface) {
  NS_LOG_FUNCTION(this);
  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  Ptr<NetDevice> dev = ipv4->GetNetDevice(iface);
  Ptr<Channel> chan = dev->GetChannel();
  Ptr<NetDevice> otherDev;
  for(uint32_t i=0; i < chan->GetNDevices(); i++) {
    otherDev = chan->GetDevice(i);
    if (otherDev->GetAddress() != dev->GetAddress()) break;
  }
  return otherDev->GetNode();
}

void 
UdpGossip::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  m_myaddr = GetSomeAddrOf(GetNode());
  m_dmc_data->SetMyIpv4Address(m_myaddr);

  if (m_d3_output) {
    Ipv4Address a(m_myaddr);
    NS_LOG_INFO("D3 nodes.push({index:" << GetNode()->GetId() <<
                ",addr:\"" << a << "\"});");
  }

  // set up receive socket
  if (m_recv_socket == 0) {
    NS_LOG_DEBUG("Setting up receiving socket");
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_recv_socket = Socket::CreateSocket (GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_recv_socket->Bind(local);
  }
  m_recv_socket->SetRecvCallback(MakeCallback(&UdpGossip::HandleRead, this));

  // set up peer sockets
  m_num_peers = GetNumPeers();
  m_peer_addresses = new Ipv4Address[m_num_peers];
  m_send_sockets = new Ptr<Socket>[m_num_peers];
  m_peer_nodes = new Ptr<Node>[m_num_peers];
  NS_LOG_DEBUG("setting up " << m_num_peers << " peers");

  Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
  uint32_t peers = 0;
  for(uint32_t iface = 0; iface < ipv4->GetNInterfaces(); iface++) {
    if (!IsLoopbackInterface(iface)) {
      Ipv4Address peerAddr = GetPeerAddr(iface);
      Ptr<Socket> ssock = m_send_sockets[peers];
      if (ssock == 0) {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        ssock = Socket::CreateSocket (GetNode (), tid);

        m_peer_addresses[peers] = peerAddr;
        m_peer_nodes[peers] = GetPeerNode(iface);

        ssock->Bind();
        ssock->Connect (InetSocketAddress (peerAddr, m_port));
        if (m_d3_output) {
          uint32_t src = GetNode()->GetId();
          uint32_t tgt = GetPeerNode(iface)->GetId();
          if (src < tgt) {
            NS_LOG_INFO("D3 links.push({source:" << src <<
                        ",target:" << tgt <<
                        "});");
          }
        } else {
          NS_LOG_INFO("established socket to peer node "
                      << GetPeerNode(iface)->GetId()
                      << " at " << peerAddr << ":" << m_port);
        }

        m_send_sockets[peers] = ssock;
      }
      peers++;
    }
  }

  ScheduleTransmit (MilliSeconds (rand() % 100));
}

void 
UdpGossip::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  m_dmc_data->LogMemory();

  if (m_recv_socket != 0) 
    {
      m_recv_socket->Close ();
      m_recv_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_recv_socket = 0;
    }

  uint32_t i;
  for(i=0; i < m_num_peers; i++) {
    Ptr<Socket> sock = m_send_sockets[i];
    if (sock != 0) {
      sock->Close();
      sock->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
    }
    m_send_sockets[i] = sock = 0;
  }
  
  // for(std::list<EventId>::iterator it = m_sendEvents.begin();
  //     it != m_sendEvents.end(); it++) {
  //   if (!it->IsExpired()) Simulator::Cancel(*it);
  // }
  Simulator::Cancel(m_sendEvent);
}

void 
UdpGossip::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &UdpGossip::Send, this);
}

void 
UdpGossip::Send (void)
{
  NS_LOG_FUNCTION (this);

  uint8_t *buf;

  uint32_t bufsz = m_dmc_data->GetMarshalledSize();
  buf = new uint8_t[bufsz];
  m_dmc_data->MarshalTo(buf);

  Ptr<Packet> p;
  p = Create<Packet> (buf, bufsz);

  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  
  // pick a peer to send to
  uint32_t peer_idx = rand() % m_num_peers;
  m_send_sockets[peer_idx]->Send(p);
  
  delete [] buf;

  ++m_sent;

  Ipv4Address peerAddress = m_peer_addresses[peer_idx];
  if (m_d3_output) {
    NS_LOG_INFO("D3 events.push({type:\"send\",from:" <<
                GetNode()->GetId() << ",to:" <<
                m_peer_nodes[peer_idx]->GetId() << ",time:" <<
                Simulator::Now().GetSeconds() << "});");
  } else {
    NS_LOG_INFO("SEND: to=" << peerAddress << " from=" << m_myaddr);
  }

  // continuous sending
  ScheduleTransmit(MilliSeconds (rand() % 100));
}

void
UdpGossip::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from))) {
    Ipv4Address src = InetSocketAddress::ConvertFrom(from).GetIpv4();
    uint32_t bufsz = packet->GetSize();
    uint8_t *buf = new uint8_t[bufsz];
    packet->CopyData(buf, bufsz);
    if (m_d3_output) {
      for(uint32_t i=0; i<m_num_peers; i++) {
        if (m_peer_addresses[i].IsEqual(src)) {
          NS_LOG_INFO("D3 events.push({type:\"recv\",from:" <<
                      m_peer_nodes[i]->GetId() << ",to:" <<
                      GetNode()->GetId() << ",time:" <<
                      Simulator::Now().GetSeconds() << "});");
        }
      }
    } else {
      NS_LOG_INFO("RECV: from=" << src << " sz=" << bufsz << " bytes");
    }
    m_dmc_data->MarshalFrom(buf);
    delete [] buf;
  }
  
}

} // Namespace ns3
