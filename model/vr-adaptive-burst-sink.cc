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
#include "vr-adaptive-burst-sink.h"
#include "vr-adaptive-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VrAdaptiveBurstSink");

NS_OBJECT_ENSURE_REGISTERED (VrAdaptiveBurstSink);

TypeId
VrAdaptiveBurstSink::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::VrAdaptiveBurstSink")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<VrAdaptiveBurstSink> ()
          .AddAttribute ("Local", "The Address on which to Bind the rx socket.", AddressValue (),
                         MakeAddressAccessor (&VrAdaptiveBurstSink::m_local), MakeAddressChecker ())
          .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                         TypeIdValue (UdpSocketFactory::GetTypeId ()),
                         MakeTypeIdAccessor (&VrAdaptiveBurstSink::m_tid), MakeTypeIdChecker ())
          .AddTraceSource ("FragmentRx", "A fragment has been received",
                           MakeTraceSourceAccessor (&VrAdaptiveBurstSink::m_rxFragmentTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback")
          .AddTraceSource ("BurstRx", "A burst has been successfully received",
                           MakeTraceSourceAccessor (&VrAdaptiveBurstSink::m_rxBurstTrace),
                           "ns3::BurstSink::SeqTsSizeFragCallback");
  return tid;
}

VrAdaptiveBurstSink::VrAdaptiveBurstSink ()
{
  NS_LOG_FUNCTION (this);
}

VrAdaptiveBurstSink::~VrAdaptiveBurstSink ()
{
  NS_LOG_FUNCTION (this);
}

void
VrAdaptiveBurstSink::FragmentReceived (BurstHandler &burstHandler, const Ptr<Packet> &f,
                                       const Address &from, const Address &localAddress)
{
  NS_LOG_FUNCTION (this << f);

  SeqTsSizeFragHeader header;
  f->PeekHeader (header);

  uint64_t m_fragment_size = f->GetSize ();
  Time delay = Simulator::Now () - header.GetTs ();

  m_rateBuffer.insert (std::pair<Time, std::pair<uint32_t, Time>> (
      Simulator::Now (), std::pair<uint32_t, Time> (m_fragment_size, delay)));

  DataRate instant_throughput =
      DataRate (m_fragment_size * 8 / (Simulator::Now () - m_lastFragmentTime).GetSeconds ());

  uint64_t bytes = 0;
  Time sum_time = Seconds (0);
  Time window = MilliSeconds (140);
  for (auto it = m_rateBuffer.begin (); it != m_rateBuffer.end ();)
    {
      if (it->first < Simulator::Now () - window)
        {
          it = m_rateBuffer.erase (it);
        }
      else
        {
          bytes += std::get<0> (it->second);
          sum_time += std::get<1> (it->second);
          ++it;
        }
    }

  DataRate avg_throughput = DataRate (
      bytes * 8 / (Simulator::Now () > window ? window : Simulator::Now ()).GetSeconds ());

  Time avg_delay = Seconds (sum_time.GetSeconds () / m_rateBuffer.size ());

  m_lastFragmentTime = Simulator::Now ();

  Ptr<Packet> packet = Create<Packet> (100);
  VrAdaptiveHeader responseHeader;

  double arate = (avg_throughput.GetBitRate () + instant_throughput.GetBitRate ()) / 2;

  if (avg_delay > MilliSeconds (70))
    {
      arate = arate * 0.4;
    }
  else
    {
      arate = arate * 0.8;
    }

  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " sec delay "
                        << delay.GetMilliSeconds () << " msec avg_delay "
                        << avg_delay.GetMilliSeconds () << " msec instant_throughput "
                        << instant_throughput.GetBitRate () / 1e6 << " Mbps avg_throughput "
                        << avg_throughput.GetBitRate () / 1e6 << " Mbps req_throughput "
                        << arate / 1e6 << " Mbps");

  responseHeader.SetTargetDataRate (DataRate (arate));
  packet->AddHeader (responseHeader);

  int64_t bytes_sent = m_socket->SendTo (packet, 0, from);

  if (bytes_sent == -1)
    {
      NS_FATAL_ERROR ("Cannot sent  " << bytes_sent << " errno " << m_socket->GetErrno ());
    }

  BurstSink::FragmentReceived (burstHandler, f, from, localAddress);
}

} // Namespace ns3
