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

  Time diff_delay = Seconds (0);

  if (!m_rateBuffers[from].empty ())
    {
      NS_LOG_DEBUG ("prev delay: " << std::get<1> (m_rateBuffers[from].rbegin ()->second));
      diff_delay = delay - std::get<1> (m_rateBuffers[from].rbegin ()->second);
      NS_LOG_DEBUG ("diff delay: " << diff_delay);
    }

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

  NS_LOG_DEBUG ("Curr delay: " << delay);

  DataRate avg_throughput = DataRate (bytes * 8 /
                                      (Simulator::Now () - m_started_ats[from] > window
                                           ? window
                                           : (Simulator::Now () - m_started_ats[from]))
                                          .GetSeconds ());

  Time avg_delay = Seconds (sum_time.GetSeconds () / m_rateBuffers[from].size ());

  m_lastFragmentTimes[from] = Simulator::Now ();

  Ptr<Packet> packet = Create<Packet> (100);
  VrAdaptiveHeader responseHeader;

  DataRate arate = fuzzyAlgorithm (delay, diff_delay, avg_throughput);

  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " from " << from << " sec delay "
                        << delay.GetMilliSeconds () << " msec avg_delay "
                        << avg_delay.GetMilliSeconds () << " msec instant_throughput "
                        << instant_throughput.GetBitRate () / 1e6 << " Mbps avg_throughput "
                        << avg_throughput.GetBitRate () / 1e6 << " Mbps req_throughput "
                        << arate.GetBitRate () / 1e6 << " Mbps");

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

DataRate
VrAdaptiveBurstSinkTcp::fuzzyAlgorithm (Time delay, Time diffDelay, DataRate avgRate)
{
  double slow = 0, ok = 0, fast = 0, falling = 0, steady = 0, rising = 0, r1 = 0, r2 = 0, r3 = 0,
         r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0, p2 = 0, p1 = 0, z = 0, n1 = 0, n2 = 0,
         output = 0;

  Time target_delay = MilliSeconds (30);

  double t = target_delay.GetSeconds ();
  double t_diff = target_delay.GetSeconds () / 3;

  double currDt = delay.GetSeconds ();
  double diff = diffDelay.GetSeconds ();

  if (currDt < 2 * t / 3)
    {
      slow = 1.0;
    }
  else if (currDt < t)
    {
      slow = 1 - 1 / (t / 3) * (currDt - 2 * t / 3);
      ok = 1 / (t / 3) * (currDt - 2 * t / 3);
    }
  else if (currDt < 4 * t)
    {
      ok = 1 - 1 / (3 * t) * (currDt - t);
      fast = 1 / (3 * t) * (currDt - t);
    }
  else
    {
      fast = 1;
    }

  if (diff < -2 * t_diff / 3)
    {
      falling = 1;
    }
  else if (diff < 0)
    {
      falling = 1 - 1 / (2 * t_diff / 3) * (diff + 2 * t_diff / 3);
      steady = 1 / (2 * t_diff / 3) * (diff + 2 * t_diff / 3);
    }
  else if (diff < 4 * t_diff)
    {
      steady = 1 - 1 / (4 * t_diff) * diff;
      rising = 1 / (4 * t_diff) * diff;
    }
  else
    {
      rising = 1;
    }

  r9 = std::min (slow, falling);
  r8 = std::min (ok, falling);
  r7 = std::min (fast, falling);
  r6 = std::min (slow, steady);
  r5 = std::min (ok, steady);
  r4 = std::min (fast, steady);
  r3 = std::min (slow, rising);
  r2 = std::min (ok, rising);
  r1 = std::min (fast, rising);

  p2 = std::sqrt (std::pow (r9, 2));
  p1 = std::sqrt (std::pow (r6, 2) + std::pow (r8, 2));
  z = std::sqrt (std::pow (r3, 2) + std::pow (r5, 2) + std::pow (r7, 2));
  n1 = std::sqrt (std::pow (r2, 2) + std::pow (r4, 2));
  n2 = std::sqrt (std::pow (r1, 2));

  /*output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 1.4 + p2 * 2)*/
  output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 2 + p2 * 4) / (n2 + n1 + z + p1 + p2);

  NS_LOG_DEBUG ("slow " << slow << " ok " << ok << " fast " << fast << " falling " << falling
                        << " steady " << steady << " rising " << rising << " output " << output);

  return DataRate (output * avgRate.GetBitRate ());
}

} // Namespace ns3
