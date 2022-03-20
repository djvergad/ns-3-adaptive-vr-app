/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2021 SIGNET Lab, Department of Information Engineering,
// University of Padova
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"
#include "ns3/burst-generator.h"
#include "bursty-application-tcp.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BurstyApplicationTcp");

NS_OBJECT_ENSURE_REGISTERED (BurstyApplicationTcp);

TypeId
BurstyApplicationTcp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::BurstyApplicationTcp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<BurstyApplicationTcp> ()
          .AddAttribute (
              "FragmentSize", "The size of packets sent in a burst including SeqTsSizeFragHeader",
              UintegerValue (1200), MakeUintegerAccessor (&BurstyApplicationTcp::m_fragSize),
              MakeUintegerChecker<uint32_t> (1))
          .AddAttribute ("Remote", "The address of the destination", AddressValue (),
                         MakeAddressAccessor (&BurstyApplicationTcp::m_peer), MakeAddressChecker ())
          .AddAttribute (
              "Local",
              "The Address on which to bind the socket. If not set, it is generated automatically.",
              AddressValue (), MakeAddressAccessor (&BurstyApplicationTcp::m_local),
              MakeAddressChecker ())
          .AddAttribute ("BurstGenerator", "The BurstGenerator used by this application",
                         PointerValue (0),
                         MakePointerAccessor (&BurstyApplicationTcp::m_burstGenerator),
                         MakePointerChecker<BurstGenerator> ())
          .AddAttribute ("Protocol",
                         "The type of protocol to use. This should be "
                         "a subclass of ns3::SocketFactory",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&BurstyApplicationTcp::m_socketTid),
                         MakeTypeIdChecker ())
          .AddTraceSource ("FragmentTx", "A fragment of the burst is sent",
                           MakeTraceSourceAccessor (&BurstyApplicationTcp::m_txFragmentTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstTx", "A burst of packet is created and sent",
                           MakeTraceSourceAccessor (&BurstyApplicationTcp::m_txBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

BurstyApplicationTcp::BurstyApplicationTcp ()
{
  NS_LOG_FUNCTION (this);
}

BurstyApplicationTcp::~BurstyApplicationTcp ()
{
  NS_LOG_FUNCTION (this);
}

void
BurstyApplicationTcp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // chain up
  BurstyApplication::DoDispose ();
}

void
BurstyApplicationTcp::StartApplication ()
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_socketTid);
      int ret = -1;

      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (!m_local.IsInvalid ())
        {
          NS_ABORT_MSG_IF ((Inet6SocketAddress::IsMatchingType (m_peer) &&
                            InetSocketAddress::IsMatchingType (m_local)) ||
                               (InetSocketAddress::IsMatchingType (m_peer) &&
                                Inet6SocketAddress::IsMatchingType (m_local)),
                           "Incompatible peer and local address IP version");
          ret = m_socket->Bind (m_local);
        }
      else
        {
          if (Inet6SocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind6 ();
            }
          else if (InetSocketAddress::IsMatchingType (m_peer) ||
                   PacketSocketAddress::IsMatchingType (m_peer))
            {
              ret = m_socket->Bind ();
            }
        }

      if (ret == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }

      m_socket->Connect (m_peer);
      // m_socket->SetAllowBroadcast (true);
      // m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (MakeCallback (&BurstyApplicationTcp::ConnectionSucceeded, this),
                                    MakeCallback (&BurstyApplicationTcp::ConnectionFailed, this));
      m_socket->SetSendCallback (MakeCallback (&BurstyApplicationTcp::DataSend, this));
    }
}

void
BurstyApplicationTcp::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  BurstyApplication::StopApplication ();
}

void
BurstyApplicationTcp::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  m_connected = true;

  // Ensure no pending event
  CancelEvents ();
  SendBurst ();
}

void
BurstyApplicationTcp::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_FATAL_ERROR ("Can't connect");
}

} // Namespace ns3
