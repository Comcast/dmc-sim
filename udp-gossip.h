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

#ifndef UDP_GOSSIP_H
#define UDP_GOSSIP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/address.h"
#include "ns3/dmc-data.h"
#include "dmc-data.h"

namespace ns3 {

class Socket;
class Packet;

/**
 * \ingroup udpgossip
 * \brief A Udp Gossip application
 *
 * Every packet sent should be returned by the server and received here.
 */
class UdpGossip : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  UdpGossip ();
  virtual ~UdpGossip ();

  /**
   * \brief set port to use for the protocol
   * \param port port number
   */
  void SetPort (uint16_t port);
  void SetDmcData (DmcData *dd);

protected:
  virtual void DoDispose (void);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Schedule the next packet transmission
   * \param dt time interval between packets.
   */
  void ScheduleTransmit (Time dt);

  /**
   * \brief Send a packet
   */
  void Send (void);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  Ipv4Address GetSomeAddrOf(Ptr<Node> node);
  bool IsLoopbackInterface(uint32_t iface);
  Ipv4Address GetPeerAddr(uint32_t iface);
  Ptr<Node> GetPeerNode(uint32_t iface);
  uint32_t GetNumPeers();

  Time m_interval; //!< Packet inter-send time

  uint32_t m_sent; //!< Counter for sent packets
  uint16_t m_port; //!< Port number
  bool m_d3_output;
  uint32_t m_num_peers;
  Ipv4Address *m_peer_addresses;
  Ptr<Node> *m_peer_nodes;
  Ipv4Address m_myaddr;
  Ptr<Socket> *m_send_sockets; //!< Peer sockets
  Ptr<Socket> m_recv_socket;
  
  EventId m_sendEvent;

  DmcData* m_dmc_data;

  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;
};

} // namespace ns3

#endif /* UDP_GOSSIP_H */
