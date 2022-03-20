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

#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/pointer.h"
#include "vr-adaptive-header.h"
#include "vr-burst-generator.h"
#include "vr-adaptive-bursty-application-tcp.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VrAdaptiveBurstyApplicationTcp");

NS_OBJECT_ENSURE_REGISTERED (VrAdaptiveBurstyApplicationTcp);

TypeId
VrAdaptiveBurstyApplicationTcp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::VrAdaptiveBurstyApplicationTcp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<VrAdaptiveBurstyApplicationTcp> ()
          .AddAttribute ("FragmentSize",
                         "The size of packets sent in a burst including SeqTsSizeFragHeader",
                         UintegerValue (1200),
                         MakeUintegerAccessor (&VrAdaptiveBurstyApplicationTcp::m_fragSize),
                         MakeUintegerChecker<uint32_t> (1))
          .AddAttribute ("Remote", "The address of the destination", AddressValue (),
                         MakeAddressAccessor (&VrAdaptiveBurstyApplicationTcp::m_peer),
                         MakeAddressChecker ())
          .AddAttribute (
              "Local",
              "The Address on which to bind the socket. If not set, it is generated automatically.",
              AddressValue (), MakeAddressAccessor (&VrAdaptiveBurstyApplicationTcp::m_local),
              MakeAddressChecker ())
          .AddAttribute ("BurstGenerator", "The BurstGenerator used by this application",
                         PointerValue (0),
                         MakePointerAccessor (&VrAdaptiveBurstyApplicationTcp::m_burstGenerator),
                         MakePointerChecker<BurstGenerator> ())
          .AddAttribute ("Protocol",
                         "The type of protocol to use. This should be "
                         "a subclass of ns3::SocketFactory",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&VrAdaptiveBurstyApplicationTcp::m_socketTid),
                         MakeTypeIdChecker ())
          .AddTraceSource (
              "FragmentTx", "A fragment of the burst is sent",
              MakeTraceSourceAccessor (&VrAdaptiveBurstyApplicationTcp::m_txFragmentTrace),
              "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource (
              "BurstTx", "A burst of packet is created and sent",
              MakeTraceSourceAccessor (&VrAdaptiveBurstyApplicationTcp::m_txBurstTrace),
              "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

VrAdaptiveBurstyApplicationTcp::VrAdaptiveBurstyApplicationTcp ()
{
  NS_LOG_FUNCTION (this);
}

VrAdaptiveBurstyApplicationTcp::~VrAdaptiveBurstyApplicationTcp ()
{
  NS_LOG_FUNCTION (this);
}

void
VrAdaptiveBurstyApplicationTcp::StartApplication ()
{
  NS_LOG_FUNCTION (this);
  BurstyApplication::StartApplication ();
  m_socket->SetRecvCallback (MakeCallback (&VrAdaptiveBurstyApplicationTcp::HandleRead, this));
  m_socket->SetConnectCallback (
      MakeCallback (&VrAdaptiveBurstyApplicationTcp::ConnectionSucceeded, this),
      MakeCallback (&VrAdaptiveBurstyApplicationTcp::ConnectionFailed, this));
}

void
VrAdaptiveBurstyApplicationTcp::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_DEBUG ("Received packet " << packet->GetSize ());
      VrAdaptiveHeader header;
      if (packet->GetSize () > header.GetSerializedSize ())
        {
          packet->RemoveHeader (header);

          NS_LOG_DEBUG ("header " << header.GetTargetDataRate ());

          if (header.GetTargetDataRate () > 0)
            {
              Ptr<VrBurstGenerator> vrBurstGenerator =
                  DynamicCast<VrBurstGenerator> (m_burstGenerator);
              if (initRate == 0)
                {
                  initRate = vrBurstGenerator->GetTargetDataRate ();
                }

              DataRate new_rate = std::min (header.GetTargetDataRate (), initRate);
              NS_LOG_DEBUG ("New rate: " << new_rate.GetBitRate () / 1.e6 << "Mbps");
              vrBurstGenerator->SetTargetDataRate (new_rate);
            }
        }
    }
}

void
VrAdaptiveBurstyApplicationTcp::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  m_connected = true;

  Ptr<VrBurstGenerator> vrBurstGenerator = DynamicCast<VrBurstGenerator> (m_burstGenerator);
  initRate = vrBurstGenerator->GetTargetDataRate ();
  vrBurstGenerator->SetTargetDataRate(DataRate(1e6));


  // Ensure no pending event
  CancelEvents ();
  SendBurst ();
}

} // Namespace ns3
