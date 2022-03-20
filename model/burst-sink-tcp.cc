/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 SIGNET Lab, Department of Information Engineering,
 * University of Padova
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "burst-sink-tcp.h"
#include "ns3/boolean.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BurstSinkTcp");

NS_OBJECT_ENSURE_REGISTERED (BurstSinkTcp);

TypeId
BurstSinkTcp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::BurstSinkTcp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<BurstSinkTcp> ()
          .AddAttribute ("Local", "The Address on which to Bind the rx socket.", AddressValue (),
                         MakeAddressAccessor (&BurstSinkTcp::m_local), MakeAddressChecker ())
          .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&BurstSinkTcp::m_tid), MakeTypeIdChecker ())
          .AddTraceSource ("FragmentRx", "A fragment has been received",
                           MakeTraceSourceAccessor (&BurstSinkTcp::m_rxFragmentTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstRx", "A burst has been successfully received",
                           MakeTraceSourceAccessor (&BurstSinkTcp::m_rxBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

BurstSinkTcp::BurstSinkTcp ()
{
  NS_LOG_FUNCTION (this);
}

BurstSinkTcp::~BurstSinkTcp ()
{
  NS_LOG_FUNCTION (this);
}

void
BurstSinkTcp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}

// Application Methods
void
BurstSinkTcp::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      if (m_socket->Bind (m_local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_socket->Listen ();
      // m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&BurstSinkTcp::HandleRead, this));
  m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                               MakeCallback (&BurstSinkTcp::HandleAccept, this));
  m_socket->SetCloseCallbacks (MakeCallback (&BurstSinkTcp::HandlePeerClose, this),
                               MakeCallback (&BurstSinkTcp::HandlePeerError, this));
}

void
BurstSinkTcp::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  BurstSink::StopApplication ();
}

void
BurstSinkTcp::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> fragment;
  Address from;
  Address localAddress;

  while ((fragment = socket->RecvFrom (from)))
    {

      if (fragment->GetSize () == 0)
        { //EOF
          break;
        }

      m_totRxBytes += fragment->GetSize ();

      if (m_incomplete_packets[socket] && m_incomplete_packets[socket]->GetSize () > 0)
        {
          m_incomplete_packets[socket]->AddAtEnd (fragment);
          fragment = m_incomplete_packets[socket]->Copy ();
          m_incomplete_packets[socket] = nullptr;
        }

      std::stringstream addressStr;
      if (InetSocketAddress::IsMatchingType (from))
        {
          addressStr << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port "
                     << InetSocketAddress::ConvertFrom (from).GetPort ();
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          addressStr << Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port "
                     << Inet6SocketAddress::ConvertFrom (from).GetPort ();
        }
      else
        {
          addressStr << "UNKNOWN ADDRESS TYPE";
        }

      NS_LOG_INFO ("At time " << Simulator::Now ().As (Time::S) << " burst sink received "
                              << fragment->GetSize () << " bytes from " << addressStr.str ()
                              << fragment->GetSize () << " bytes from "
                              << " total Rx " << m_totRxBytes << " bytes");

      socket->GetSockName (localAddress);

      // handle received fragment
      auto itBuffer = m_burstHandlerMap.find (from); // rename m_burstBufferMap, itBuffer
      if (itBuffer == m_burstHandlerMap.end ())
        {
          NS_LOG_LOGIC ("New stream from " << from);
          itBuffer = m_burstHandlerMap.insert (std::make_pair (from, BurstHandler ())).first;
        }

      uint64_t del_size = 0;

      SeqTsSizeFragHeader header;
      if (header.GetSerializedSize () <= fragment->GetSize ())
        {
          fragment->PeekHeader (header);
          NS_LOG_DEBUG ("HEADER SIZE " << header.GetSize () << " header seriqlized "
                                       << header.GetSerializedSize () << " fragment bytes "
                                       << header.GetFragBytes () << " fragment size "
                                       << fragment->GetSize () << " fragment serialized "
                                       << fragment->GetSerializedSize ());
          del_size = header.GetFragBytes ();

          if (del_size <= fragment->GetSize ())
            {

              m_incomplete_packets[socket] =
                  fragment->Copy ()->CreateFragment (del_size, fragment->GetSize () - del_size);
              fragment = fragment->Copy ()->CreateFragment (0, del_size);

              FragmentReceived (itBuffer->second, fragment, from, localAddress);
            }
          else
            {
              m_incomplete_packets[socket] = fragment->Copy ();
            }
        }
      else
        {
          NS_LOG_DEBUG ("HEADER SIZE TOO SMALL: fragment size " << fragment->GetSize ()
                                                                << " fragment serialized "
                                                                << fragment->GetSerializedSize ());
          m_incomplete_packets[socket] = fragment->Copy ();
        }
    }
}

} // Namespace ns3
