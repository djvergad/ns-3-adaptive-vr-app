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
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "vr-adaptive-burst-sink-tcp.h"
#include "vr-adaptive-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VrAdaptiveBurstSinkTcp");

NS_OBJECT_ENSURE_REGISTERED (VrAdaptiveBurstSinkTcp);

TypeId
VrAdaptiveBurstSinkTcp::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::VrAdaptiveBurstSinkTcp")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<VrAdaptiveBurstSinkTcp> ()
          .AddAttribute ("Local", "The Address on which to Bind the rx socket.", AddressValue (),
                         MakeAddressAccessor (&VrAdaptiveBurstSinkTcp::m_local),
                         MakeAddressChecker ())
          .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&VrAdaptiveBurstSinkTcp::m_tid), MakeTypeIdChecker ())
          .AddTraceSource ("FragmentRx", "A fragment has been received",
                           MakeTraceSourceAccessor (&VrAdaptiveBurstSinkTcp::m_rxFragmentTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstRx", "A burst has been successfully received",
                           MakeTraceSourceAccessor (&VrAdaptiveBurstSinkTcp::m_rxBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

VrAdaptiveBurstSinkTcp::VrAdaptiveBurstSinkTcp ()
{
  NS_LOG_FUNCTION (this);
}

VrAdaptiveBurstSinkTcp::~VrAdaptiveBurstSinkTcp ()
{
  NS_LOG_FUNCTION (this);
}

void
VrAdaptiveBurstSinkTcp::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  m_tempSocket = socket;

  BurstSinkTcp::HandleRead (socket);
}

void
VrAdaptiveBurstSinkTcp::FragmentReceived (BurstHandler &burstHandler, const Ptr<Packet> &f,
                                          const Address &from, const Address &localAddress)
{
  NS_LOG_FUNCTION (this << f);

  DataRate arate = m_fuzzyAlgorithms[from].fragmentReceived(f);

  Ptr<Packet> packet = Create<Packet> (100);
  VrAdaptiveHeader responseHeader;


  responseHeader.SetTargetDataRate (arate);
  packet->AddHeader (responseHeader);

  int64_t bytes_sent = m_tempSocket->Send (packet);

  if (bytes_sent == -1)
    {
      int my_errno = m_socket->GetErrno ();
      if (my_errno != Socket::ERROR_NOTERROR)
        {
          NS_FATAL_ERROR ("Cannot sent  " << bytes_sent << " errno " << my_errno);
        }
    }
  // }

  BurstSinkTcp::FragmentReceived (burstHandler, f, from, localAddress);
}

} // Namespace ns3
