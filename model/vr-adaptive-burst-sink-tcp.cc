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

  SeqTsSizeFragHeader header;
  f->PeekHeader (header);

  uint64_t m_fragment_size = f->GetSize ();

  // std::cout << "Received fragment size " << m_fragment_size << " Now " << Simulator::Now ()
  //           << " last frag time " << m_lastFragmentTime << std::endl;

  if (m_started_ats[from] == Seconds (0))
    {
      m_started_ats[from] = Simulator::Now ();
    }

  // else if (Simulator::Now () != m_lastFragmentTimes[from])
  //   {

  Time delay = Simulator::Now () - header.GetTs ();

  m_rateBuffers[from].insert (std::pair<Time, std::pair<uint32_t, Time>> (
      Simulator::Now (), std::pair<uint32_t, Time> (m_fragment_size, delay)));

  DataRate instant_throughput = DataRate (
      m_fragment_size * 8 / (Simulator::Now () - m_lastFragmentTimes[from]).GetSeconds ());

  uint64_t bytes = 0;
  Time sum_time = Seconds (0);
  Time window = MilliSeconds (140);
  for (auto it = m_rateBuffers[from].begin (); it != m_rateBuffers[from].end ();)
    {
      if (it->first < Simulator::Now () - window)
        {
          it = m_rateBuffers[from].erase (it);
        }
      else
        {
          bytes += std::get<0> (it->second);
          sum_time += std::get<1> (it->second);
          ++it;
        }
    }

  DataRate avg_throughput = DataRate (bytes * 8 /
                                      (Simulator::Now () - m_started_ats[from] > window
                                           ? window
                                           : (Simulator::Now () - m_started_ats[from]))
                                          .GetSeconds ());

  Time avg_delay = Seconds (sum_time.GetSeconds () / m_rateBuffers[from].size ());

  m_lastFragmentTimes[from] = Simulator::Now ();

  Ptr<Packet> packet = Create<Packet> (100);
  VrAdaptiveHeader responseHeader;

  // double arate = 0.7 * avg_throughput.GetBitRate () + 0.3 * instant_throughput.GetBitRate ();

  double arate = avg_throughput.GetBitRate ();

  // double bdp = avg_delay.GetSeconds () * arate;

  if (std::max (avg_delay, delay) > MilliSeconds (30))
    {
      arate = arate * 0.4;
    }
  else if (std::max (avg_delay, delay) > MilliSeconds (20))
    {
      arate = arate * 0.8;
    }
  else if (std::max (avg_delay, delay) < MilliSeconds (2))
    {
      arate = arate * 1.4;
    }
  else if (avg_delay < MilliSeconds (5))
    {
      arate = arate * 1.2;
    }
  else
    {
      arate = arate * 1.05;
    }

  // if (avg_delay > MilliSeconds (70))
  //   {
  //     arate = arate * 0.4;
  //   }
  // else
  //   {
  //     arate = arate * 0.8;
  //   }

  // std::cout << "Time " << Simulator::Now ().GetSeconds () << " sec delay "
  //           << delay.GetMilliSeconds () << " msec avg_delay " << avg_delay.GetMilliSeconds ()
  //           << " msec instant_throughput " << instant_throughput.GetBitRate () / 1e6
  //           << " Mbps avg_throughput " << avg_throughput.GetBitRate () / 1e6
  //           << " Mbps req_throughput " << arate / 1e6 << " Mbps" << std::endl;

  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " from " << from << " sec delay "
                        << delay.GetMilliSeconds () << " msec avg_delay "
                        << avg_delay.GetMilliSeconds () << " msec instant_throughput "
                        << instant_throughput.GetBitRate () / 1e6 << " Mbps avg_throughput "
                        << avg_throughput.GetBitRate () / 1e6 << " Mbps req_throughput "
                        << arate / 1e6 << " Mbps");

  responseHeader.SetTargetDataRate (DataRate (arate));
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
