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
#include "vr-adaptive-bursty-application.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VrAdaptiveBurstyApplication");

NS_OBJECT_ENSURE_REGISTERED (VrAdaptiveBurstyApplication);

TypeId
VrAdaptiveBurstyApplication::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::VrAdaptiveBurstyApplication")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<VrAdaptiveBurstyApplication> ()
          .AddAttribute (
              "FragmentSize", "The size of packets sent in a burst including SeqTsSizeFragHeader",
              UintegerValue (1200), MakeUintegerAccessor (&VrAdaptiveBurstyApplication::m_fragSize),
              MakeUintegerChecker<uint32_t> (1))
          .AddAttribute ("Remote", "The address of the destination", AddressValue (),
                         MakeAddressAccessor (&VrAdaptiveBurstyApplication::m_peer),
                         MakeAddressChecker ())
          .AddAttribute (
              "Local",
              "The Address on which to bind the socket. If not set, it is generated automatically.",
              AddressValue (), MakeAddressAccessor (&VrAdaptiveBurstyApplication::m_local),
              MakeAddressChecker ())
          .AddAttribute ("BurstGenerator", "The BurstGenerator used by this application",
                         PointerValue (0),
                         MakePointerAccessor (&VrAdaptiveBurstyApplication::m_burstGenerator),
                         MakePointerChecker<BurstGenerator> ())
          .AddAttribute ("Protocol",
                         "The type of protocol to use. This should be "
                         "a subclass of ns3::SocketFactory",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&VrAdaptiveBurstyApplication::m_socketTid),
                         MakeTypeIdChecker ())
          .AddTraceSource (
              "FragmentTx", "A fragment of the burst is sent",
              MakeTraceSourceAccessor (&VrAdaptiveBurstyApplication::m_txFragmentTrace),
              "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstTx", "A burst of packet is created and sent",
                           MakeTraceSourceAccessor (&VrAdaptiveBurstyApplication::m_txBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

VrAdaptiveBurstyApplication::VrAdaptiveBurstyApplication ()
{
  NS_LOG_FUNCTION (this);
}

VrAdaptiveBurstyApplication::~VrAdaptiveBurstyApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
VrAdaptiveBurstyApplication::StartApplication ()
{
  NS_LOG_FUNCTION (this);
  BurstyApplication::StartApplication ();
  m_socket->SetRecvCallback (MakeCallback (&VrAdaptiveBurstyApplication::HandleRead, this));
}

void
VrAdaptiveBurstyApplication::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      VrAdaptiveHeader header;
      packet->RemoveHeader (header);

      Ptr<VrBurstGenerator> vrBurstGenerator = DynamicCast<VrBurstGenerator> (m_burstGenerator);
      if (initRate == 0 ) {
        initRate = vrBurstGenerator->GetTargetDataRate();
      }

      DataRate new_rate = std::min(header.GetTargetDataRate (), initRate);
      // std::cout << "New rate: " << new_rate << std::endl;
      vrBurstGenerator->SetTargetDataRate (new_rate);
    }
}

} // Namespace ns3
