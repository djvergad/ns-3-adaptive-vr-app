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
#include "bursty-application-client.h"
#include "ns3/packet-socket-address.h"
#include "ns3/boolean.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BurstyApplicationClient");

NS_OBJECT_ENSURE_REGISTERED (BurstyApplicationClient);

TypeId
BurstyApplicationClient::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::BurstyApplicationClient")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<BurstyApplicationClient> ()
          .AddAttribute ("Local", "The Address on which to Bind the rx socket.", AddressValue (),
                         MakeAddressAccessor (&BurstyApplicationClient::m_local),
                         MakeAddressChecker ())

          .AddAttribute ("Remote", "The address of the destination", AddressValue (),
                         MakeAddressAccessor (&BurstyApplicationClient::m_peer),
                         MakeAddressChecker ())

          .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&BurstyApplicationClient::m_tid), MakeTypeIdChecker ())
          .AddTraceSource ("FragmentRx", "A fragment has been received",
                           MakeTraceSourceAccessor (&BurstyApplicationClient::m_rxFragmentTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstRx", "A burst has been successfully received",
                           MakeTraceSourceAccessor (&BurstyApplicationClient::m_rxBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

BurstyApplicationClient::BurstyApplicationClient ()
{
  NS_LOG_FUNCTION (this);
}

BurstyApplicationClient::~BurstyApplicationClient ()
{
  NS_LOG_FUNCTION (this);
}

uint64_t
BurstyApplicationClient::GetTotalRxBytes () const
{
  NS_LOG_FUNCTION (this);
  return m_totRxBytes;
}

uint64_t
BurstyApplicationClient::GetTotalRxFragments () const
{
  NS_LOG_FUNCTION (this);
  return m_totRxFragments;
}

uint64_t
BurstyApplicationClient::GetTotalRxBursts () const
{
  NS_LOG_FUNCTION (this);
  return m_totRxBursts;
}

void
BurstyApplicationClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  // chain up
  Application::DoDispose ();
}

// Application Methods
void
BurstyApplicationClient::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      int ret = -1;

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

      NS_LOG_DEBUG ("Trying to connect");
      int res = m_socket->Connect (m_peer);
      NS_LOG_DEBUG ("After connect, " << res << " " << m_socket->GetErrno ());

      // m_socket->SetAllowBroadcast (true);
      // m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (
          MakeCallback (&BurstyApplicationClient::ConnectionSucceeded, this),
          MakeCallback (&BurstyApplicationClient::ConnectionFailed, this));

      m_socket->SetCloseCallbacks (MakeCallback (&BurstyApplicationClient::HandlePeerClose, this),
                                   MakeCallback (&BurstyApplicationClient::HandlePeerError, this));

      m_socket->SetRecvCallback (MakeCallback (&BurstyApplicationClient::HandleRead, this));

      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          Ptr<Packet> dummy = Create<Packet> (100);
          m_socket->Send (dummy);
        }
    }
}

void
BurstyApplicationClient::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
    }
}

void
BurstyApplicationClient::HandleRead (Ptr<Socket> socket)
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
              NS_LOG_DEBUG ("Incomplete packet in rxbuffer, received. size: " << m_incomplete_packets[socket]->GetSize() << " hsize " << del_size);

              FragmentReceived (itBuffer->second, fragment, from, localAddress);
            }
          else
            {
              m_incomplete_packets[socket] = fragment->Copy ();
              NS_LOG_DEBUG ("Incomplete packet in rxbuffer, not received. size: " << m_incomplete_packets[socket]->GetSize() << " hsize " << del_size);
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

void
BurstyApplicationClient::FragmentReceived (BurstHandler &burstHandler, const Ptr<Packet> &f,
                                           const Address &from, const Address &localAddress)
{
  NS_LOG_FUNCTION (this << f);

  SeqTsSizeFragHeader header;
  f->PeekHeader (header);
  NS_ABORT_IF (header.GetSize () == 0);

  m_totRxFragments++;
  m_rxFragmentTrace (f, localAddress, from,
                     header); // TODO should fragment still include header in trace?

  NS_LOG_DEBUG ("Get BurstHandler for from="
                << from << " with m_currentBurstSeq=" << burstHandler.m_currentBurstSeq
                << ", m_fragmentsMerged=" << burstHandler.m_fragmentsMerged
                << ", m_unorderedFragments.size ()=" << burstHandler.m_unorderedFragments.size ()
                << ", m_burstBuffer.GetSize ()=" << burstHandler.m_burstBuffer->GetSize ()
                << ", for fragment with header: " << header);

  if (header.GetSeq () < burstHandler.m_currentBurstSeq)
    {
      NS_LOG_LOGIC ("Ignoring fragment from previous burst. Fragment burst seq="
                    << header.GetSeq ()
                    << ", current burst seq=" << burstHandler.m_currentBurstSeq);
      return;
    }

  if (header.GetSeq () > burstHandler.m_currentBurstSeq)
    {
      // fragment of new burst: discard previous burst if incomplete
      NS_LOG_LOGIC ("Start mering new burst seq "
                    << header.GetSeq () << " (previous=" << burstHandler.m_currentBurstSeq << ")");

      burstHandler.m_currentBurstSeq = header.GetSeq ();
      burstHandler.m_fragmentsMerged = 0;
      burstHandler.m_unorderedFragments.clear ();
      burstHandler.m_burstBuffer = Create<Packet> (0);
    }

  if (header.GetSeq () == burstHandler.m_currentBurstSeq)
    {
      // fragment of current burst
      NS_ASSERT_MSG (header.GetFragSeq () >= burstHandler.m_fragmentsMerged,
                     header.GetFragSeq () << " >= " << burstHandler.m_fragmentsMerged);

      NS_LOG_DEBUG ("fragment sequence=" << header.GetFragSeq () << ", fragments merged="
                                         << burstHandler.m_fragmentsMerged);
      if (header.GetFragSeq () == burstHandler.m_fragmentsMerged)
        {
          // following packet: merge it
          f->RemoveHeader (header);
          burstHandler.m_burstBuffer->AddAtEnd (f);
          burstHandler.m_fragmentsMerged++;
          NS_LOG_LOGIC ("Fragments merged " << burstHandler.m_fragmentsMerged << "/"
                                            << header.GetFrags () << " for burst "
                                            << header.GetSeq ());

          // if present, merge following unordered fragments
          auto nextFragmentIt = burstHandler.m_unorderedFragments.begin ();
          while (nextFragmentIt !=
                     burstHandler.m_unorderedFragments.end () && // there are unordered packets
                 nextFragmentIt->first ==
                     burstHandler.m_fragmentsMerged) // the following fragment was already received
            {
              Ptr<Packet> storedFragment = nextFragmentIt->second;
              storedFragment->RemoveHeader (header);
              burstHandler.m_burstBuffer->AddAtEnd (storedFragment);
              burstHandler.m_fragmentsMerged++;
              NS_LOG_LOGIC ("Unordered fragments merged " << burstHandler.m_fragmentsMerged << "/"
                                                          << header.GetFrags () << " for burst "
                                                          << header.GetSeq ());

              nextFragmentIt = burstHandler.m_unorderedFragments.erase (nextFragmentIt);
            }
        }
      else
        {
          // add to unordered fragments buffer
          NS_LOG_LOGIC ("Add unordered fragment " << header.GetFragSeq () << " of burst "
                                                  << header.GetSeq () << " to buffer ");
          burstHandler.m_unorderedFragments.insert (
              std::pair<uint16_t, const Ptr<Packet>> (header.GetFragSeq (), f));
        }
    }

  // check if burst is complete
  if (burstHandler.m_fragmentsMerged == header.GetFrags ())
    {
      // all fragments have been merged
      NS_ASSERT_MSG (burstHandler.m_burstBuffer->GetSize () == header.GetSize (),
                     burstHandler.m_burstBuffer->GetSize () << " == " << header.GetSize ());

      NS_LOG_LOGIC ("Burst received: " << header.GetFrags () << " fragments for a total of "
                                       << header.GetSize () << " B " << header.GetSeq ());
      m_totRxBursts++;
      m_rxBurstTrace (burstHandler.m_burstBuffer, localAddress, from,
                      header); // TODO header size does not include payload, why?
    }
}

void
BurstyApplicationClient::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void
BurstyApplicationClient::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void
BurstyApplicationClient::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void
BurstyApplicationClient::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
} // Namespace ns3
