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

#include "bursty-application-server-instance.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/burst-generator.h"
#include "ns3/data-rate.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BurstyApplicationServerInstance");

NS_OBJECT_ENSURE_REGISTERED(BurstyApplicationServerInstance);

TypeId
BurstyApplicationServerInstance::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::BurstyApplicationServerInstance")
            .SetParent<Object>()
            .SetGroupName("Applications")
            .AddConstructor<BurstyApplicationServerInstance>()
            .AddAttribute("FragmentSize",
                          "The size of packets sent in a burst including SeqTsSizeFragHeader",
                          UintegerValue(1200),
                          MakeUintegerAccessor(&BurstyApplicationServerInstance::m_fragSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&BurstyApplicationServerInstance::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&BurstyApplicationServerInstance::m_local),
                          MakeAddressChecker())
            .AddAttribute("BurstGenerator",
                          "The BurstGenerator used by this application",
                          PointerValue(0),
                          MakePointerAccessor(&BurstyApplicationServerInstance::m_burstGenerator),
                          MakePointerChecker<BurstGenerator>())
            .AddAttribute("Protocol",
                          "The type of protocol to use. This should be "
                          "a subclass of ns3::SocketFactory",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&BurstyApplicationServerInstance::m_socketTid),
                          MakeTypeIdChecker())
        // .AddTraceSource ("FragmentTx", "A fragment of the burst is sent",
        //                  MakeTraceSourceAccessor
        //                  (&BurstyApplicationServerInstance::m_txFragmentTrace),
        //                  "ns3::BurstSink::SeqTsSizeFragCallback")
        // .AddTraceSource ("BurstTx", "A burst of packet is created and sent",
        //                  MakeTraceSourceAccessor
        //                  (&BurstyApplicationServerInstance::m_txBurstTrace),
        //                  "ns3::BurstSink::SeqTsSizeFragCallback")
        ;
    return tid;
}

BurstyApplicationServerInstance::BurstyApplicationServerInstance()
    : m_connected(false),
      m_totTxBursts(0),
      m_totTxFragments(0),
      m_totTxBytes(0)

{
    NS_LOG_FUNCTION(this);
}

// BurstyApplicationServerInstance::BurstyApplicationServerInstance (
//     Ptr<Socket> socket, Address peer,
//     TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader
//     &>
//         txBurstTrace,
//     TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader
//     &>
//         txFragmentTrace)
//     : m_socket (socket),
//       m_peer (peer),
//       m_connected (false),
//       m_totTxBursts (0),
//       m_totTxFragments (0),
//       m_totTxBytes (0),
//       m_txBurstTrace (txBurstTrace),
//       m_txFragmentTrace (txFragmentTrace)
// {
//   NS_LOG_FUNCTION (this);
// }

BurstyApplicationServerInstance::~BurstyApplicationServerInstance()
{
    NS_LOG_FUNCTION(this);
}

Ptr<Socket>
BurstyApplicationServerInstance::GetSocket(void) const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

Ptr<BurstGenerator>
BurstyApplicationServerInstance::GetBurstGenerator(void) const
{
    return m_burstGenerator;
}

void
BurstyApplicationServerInstance::DoDispose(void)
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    m_socket = 0;
    m_burstGenerator = 0;
}

void
BurstyApplicationServerInstance::CancelEvents()
{
    NS_LOG_FUNCTION(this);

    // Cancel next burst event
    Simulator::Cancel(m_nextBurstEvent);
}

void
BurstyApplicationServerInstance::StopBursts(void)
{
    NS_LOG_FUNCTION(this);
    m_isfinishing = true;
}

void
BurstyApplicationServerInstance::AdaptRate()
{
    NS_LOG_FUNCTION(this);

    if (m_txStarted != Seconds(0))
    {
        m_txTime += Simulator::Now() - m_txStarted;
    }

    DataRate nextDataRate = std::max(
        m_adaptationAlgorithmServer->nextBurstRate(m_socket, m_bytesAddedToSocket, m_txTime),
        DataRate("3128000"));

    m_txTime = Seconds(0);

    UintegerValue buf_size;
    DynamicCast<TcpSocketBase>(m_socket)->GetAttribute("SndBufSize", buf_size);

    if (m_queue.size() == 0 && buf_size.Get() == m_socket->GetTxAvailable())
    {
        m_txStarted = Seconds(0);
    }
    else
    {
        m_txStarted = Simulator::Now();
    }

    DynamicCast<VrBurstGenerator>(m_burstGenerator)
        ->SetTargetDataRate(std::min(nextDataRate, m_initRate));

    std::stringstream addressStr;
    if (InetSocketAddress::IsMatchingType(m_peer))
    {
        addressStr << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                   << InetSocketAddress::ConvertFrom(m_peer).GetPort();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peer))
    {
        addressStr << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                   << Inet6SocketAddress::ConvertFrom(m_peer).GetPort();
    }
    else
    {
        addressStr << "UNKNOWN ADDRESS TYPE";
    }

    std::cout << Simulator::Now().GetSeconds() << " peer " << addressStr.str() << " nextNoLimit "
              << nextDataRate.GetBitRate() / 1.e6 << " nextdatarate "
              << DynamicCast<VrBurstGenerator>(m_burstGenerator)->GetTargetDataRate().GetBitRate() /
                     1e6
              << std::endl;
    m_bytesAddedToSocket = 0;
}

void
BurstyApplicationServerInstance::SendBurst()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_nextBurstEvent.IsExpired());

    Time period = MilliSeconds(16);

    if (!m_isfinishing)
    {
        if (m_adaptationAlgorithmServer)
        {
            AdaptRate();
        }

        std::stringstream addressStr;
        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            addressStr << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                       << InetSocketAddress::ConvertFrom(m_peer).GetPort();
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peer))
        {
            addressStr << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                       << Inet6SocketAddress::ConvertFrom(m_peer).GetPort();
        }
        else
        {
            addressStr << "UNKNOWN ADDRESS TYPE";
        }

        NS_LOG_INFO(Simulator::Now().GetSeconds()
                    << " peer " << addressStr.str() << " rate "
                    << (DynamicCast<VrBurstGenerator>(GetBurstGenerator()))
                               ->GetTargetDataRate()
                               .GetBitRate() /
                           1e6);

        DataSend(m_socket, 0);

        // get burst info
        uint32_t burstSize = 0;
        // packets must be at least as big as the header
        // while (burstSize < 24) // TODO: find a way to improve this
        //{
        if (!m_burstGenerator->HasNextBurst())
        {
            NS_LOG_LOGIC("Burst generator has no next burst: stopping application");
            StopApplication();
            return;
        }

        std::tie(burstSize, period) = m_burstGenerator->GenerateBurst();
        NS_LOG_DEBUG("Generated burstSize=" << burstSize << ", period=" << period.As(Time::MS));
        //}
        //
        SeqTsSizeFragHeader hdrTmp;

        uint32_t min_burst = 3000;

        if (burstSize < min_burst)
        {
            burstSize = min_burst;
        }

        NS_ASSERT_MSG(period.IsPositive(),
                      "Period must be non-negative, instead found period=" << period.As(Time::S));

        // send packets for current burst
        SendFragmentedBurst(burstSize);

        // schedule next burst
        NS_LOG_DEBUG("Next burst scheduled in " << period.As(Time::S));
    }
    else
    {
        // for (int i = 0; i < 100; i++)
        //   {
        //     Ptr<Packet> dummy = Create<Packet> (500);
        //     SeqTsSizeFragHeader header;
        //     header.SetFragBytes (dummy->GetSize () + header.GetSerializedSize());
        //     header.SetSeq (UINT32_MAX);
        //     dummy->AddHeader (header);
        //     m_socket->Send (dummy);
        //   }
    }
    m_nextBurstEvent =
        Simulator::Schedule(period, &BurstyApplicationServerInstance::SendBurst, this);
    DataSend(m_socket, 0);
    // UintegerValue buf_size;
    // m_socket->GetAttribute ("SndBufSize", buf_size);
    // uint64_t buff_occ = buf_size.Get () - m_socket->GetTxAvailable ();
    // std::cout << Simulator::Now ().GetSeconds () << " SndBufSize " << buf_size.Get () << " avail
    // "
    //           << m_socket->GetTxAvailable () << " occup " << buff_occ << std::endl;
}

void
BurstyApplicationServerInstance::StopApplication()
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    if (m_socket)
    {
        m_socket->Close();
    }
    else
    {
        NS_LOG_WARN("BurstyApplication found null socket to close in StopApplication");
    }
}

void
BurstyApplicationServerInstance::SendFragmentedBurst(uint32_t burstSize)
{
    NS_LOG_FUNCTION(this << burstSize);

    // prepare header
    SeqTsSizeFragHeader hdrTmp;

    NS_ABORT_MSG_IF(burstSize < hdrTmp.GetSerializedSize(),
                    burstSize << " < " << hdrTmp.GetSerializedSize());
    NS_ABORT_MSG_IF(m_fragSize < hdrTmp.GetSerializedSize(),
                    m_fragSize << " < " << hdrTmp.GetSerializedSize());

    // compute number of fragments and sizes
    uint32_t numFullFrags = burstSize / m_fragSize; // integer division
    uint32_t lastFragSize = burstSize % m_fragSize; // modulo

    uint32_t secondToLastFragSize = 0;
    if (numFullFrags > 0)
    {
        // if there is at least one full fragment, there exist a second-to-last of full size
        secondToLastFragSize = m_fragSize;
        numFullFrags--;
    }
    if (secondToLastFragSize > 0 &&                // there exist a second-to-last fragment
        lastFragSize > 0 &&                        // last smaller fragment is needed
        lastFragSize < hdrTmp.GetSerializedSize()) // the last fragment is below the minimum size
    {
        // reduce second-to-last fragment to make last fragment of minimum size
        secondToLastFragSize = m_fragSize + lastFragSize - hdrTmp.GetSerializedSize();
        lastFragSize =
            hdrTmp.GetSerializedSize(); // TODO packet with no payload: might be a problem
    }
    NS_ABORT_MSG_IF(0 < secondToLastFragSize && secondToLastFragSize < hdrTmp.GetSerializedSize(),
                    secondToLastFragSize << " < " << hdrTmp.GetSerializedSize());
    NS_ABORT_MSG_IF(0 < lastFragSize && lastFragSize < hdrTmp.GetSerializedSize(),
                    lastFragSize << " < " << hdrTmp.GetSerializedSize());

    // total number of fragments
    uint32_t totFrags = numFullFrags;
    if (secondToLastFragSize > 0)
    {
        totFrags++;
    }
    if (lastFragSize > 0)
    {
        totFrags++;
    }
    uint64_t burstPayload = burstSize - (hdrTmp.GetSerializedSize() * totFrags);
    uint64_t fullFragmentPayload = m_fragSize - hdrTmp.GetSerializedSize();
    NS_LOG_DEBUG("Current burst size: "
                 << burstSize << " B: " << totFrags << " fragments with total payload "
                 << burstPayload << " B. "
                 << "Sending fragments: " << numFullFrags << " x " << m_fragSize << "B, + "
                 << secondToLastFragSize << " B + " << lastFragSize << " B");

    // uint8_t buffer[50000];

    // for (size_t i = 0; i < burstPayload && i < sizeof (buffer); i++)
    //   {
    //     buffer[i] = 0;
    //   }

    // Ptr<Packet> burst = Create<Packet> (buffer, burstPayload);
    Ptr<Packet> burst = Create<Packet>(burstPayload);
    // Trace before adding header, for consistency with BurstSink
    Address from, to;
    m_socket->GetSockName(from);
    m_socket->GetPeerName(to);

    // TODO improve
    hdrTmp.SetSeq(m_totTxBursts);
    hdrTmp.SetSize(burstPayload);
    hdrTmp.SetFrags(totFrags);
    hdrTmp.SetFragSeq(0);

    m_txBurstTrace(burst, from, to, hdrTmp);

    uint64_t fragmentStart = 0;
    uint16_t fragmentSeq = 0;

    // if ((numFullFrags + (secondToLastFragSize > 0) + (lastFragSize > 0)) + m_queue.size () <
    //     m_queueSize)
    //   {

    for (uint32_t i = 0; i < numFullFrags; i++)
    {
        Ptr<Packet> fragment = burst->CreateFragment(fragmentStart, fullFragmentPayload);
        fragmentStart += fullFragmentPayload;
        SendFragment(fragment, burstPayload, totFrags, fragmentSeq++);
    }

    if (secondToLastFragSize > 0)
    {
        uint64_t secondToLastFragPayload = secondToLastFragSize - hdrTmp.GetSerializedSize();
        Ptr<Packet> fragment = burst->CreateFragment(fragmentStart, secondToLastFragPayload);
        fragmentStart += secondToLastFragPayload;
        SendFragment(fragment, burstPayload, totFrags, fragmentSeq++);
    }

    if (lastFragSize > 0)
    {
        uint64_t lastFragPayload = lastFragSize - hdrTmp.GetSerializedSize();
        Ptr<Packet> fragment = burst->CreateFragment(fragmentStart, lastFragPayload);
        fragmentStart += lastFragPayload;
        SendFragment(fragment, burstPayload, totFrags, fragmentSeq++);
    }
    NS_ASSERT(fragmentStart == burst->GetSize());
    // }

    m_totTxBursts++;
}

void
BurstyApplicationServerInstance::SendFragment(Ptr<Packet> fragment,
                                              uint64_t burstSize,
                                              uint16_t totFrags,
                                              uint16_t fragmentSeq)
{
    NS_LOG_FUNCTION(this << fragment << burstSize << totFrags << fragmentSeq);

    SeqTsSizeFragHeader header;
    header.SetSeq(m_totTxBursts);
    header.SetSize(burstSize);
    header.SetFrags(totFrags);
    header.SetFragSeq(fragmentSeq);
    // std::cout << "before " << fragment->GetSize () << " headersize " << header.GetSerializedSize
    // ()
    //           << std::endl;
    header.SetFragBytes(fragment->GetSize() + header.GetSerializedSize());
    // std::cout << "frag bytes " << header.GetFragBytes () << " headersize "
    //           << header.GetSerializedSize () << std::endl;

    fragment->AddHeader(header);

    // std::cout << "frag size " << fragment->GetSize () << " serialized "
    //           << fragment->GetSerializedSize () << std::endl;

    uint32_t fragmentSize = fragment->GetSize();

    // if (m_socket->GetTxAvailable () < fragment->GetSize ())
    //   {
    //     NS_LOG_DEBUG (
    //         "Unable to send fragment: TxBuffer is full, disgarding. = " << fragment->GetSize ());
    //     return;
    //   }

    if (m_queue.size() < m_queueSize)
    {
        m_queue.push_back(*fragment);
    }
    else
    {
        NS_ABORT_MSG("m_queue got full, it shouldn't");
    }
    // int actual = m_socket->Send (fragment);
    // if (uint32_t (actual) == fragmentSize)
    // {
    Address from, to = m_peer;
    m_socket->GetSockName(from);
    // m_socket->GetPeerName (to);

    m_txFragmentTrace(fragment,
                      from,
                      to,
                      header); // TODO should fragment already include header in trace?
    m_totTxFragments++;
    m_totTxBytes += fragmentSize;

    std::stringstream addressStr;
    if (InetSocketAddress::IsMatchingType(to))
    {
        addressStr << InetSocketAddress::ConvertFrom(to).GetIpv4() << " port "
                   << InetSocketAddress::ConvertFrom(to).GetPort();
    }
    else if (Inet6SocketAddress::IsMatchingType(to))
    {
        addressStr << Inet6SocketAddress::ConvertFrom(to).GetIpv6() << " port "
                   << Inet6SocketAddress::ConvertFrom(to).GetPort();
    }
    else
    {
        addressStr << "UNKNOWN ADDRESS TYPE";
    }

    NS_LOG_INFO("At time " << Simulator::Now().As(Time::S)
                           << " bursty application sent fragment of " << fragment->GetSize()
                           << " bytes to " << addressStr.str() << " with header=" << header);

    DataSend(m_socket, 0);
}

// else
//   {

//     NS_LOG_DEBUG ("Unable to send fragment: fragment size="
//                   << fragment->GetSize () << ", socket sent=" << actual << " errno "
//                   << m_socket->GetErrno () << "; ignoring unexpected behavior");
//   }
//}

void
BurstyApplicationServerInstance::DataSend(Ptr<Socket> socket, uint32_t)
{
    NS_LOG_FUNCTION(this << socket);

    // Ptr<Packet> dummy = Create<Packet> (0);
    // socket->Send (dummy);

    while (!m_queue.empty())
    {
        if (m_adaptationAlgorithmServer && m_txStarted == Seconds(0))
        {
            m_txStarted = Simulator::Now();
        }

        uint32_t max_tx_size = socket->GetTxAvailable();

        Ptr<Packet> frame = m_queue.front().Copy();
        uint32_t init_size = frame->GetSize();

        if (max_tx_size <= init_size)
        {
            // NS_ABORT_MSG ("Socket Send buffer is full");
            return;
        }

        m_queue.pop_front();

        // if (max_tx_size < init_size)
        //   {
        //     NS_LOG_INFO ("Insufficient space in send buffer, fragmenting");
        //     Ptr<Packet> frag0 = frame->CreateFragment (0, max_tx_size);
        //     Ptr<Packet> frag1 = frame->CreateFragment (max_tx_size, init_size - max_tx_size);

        //     m_queue.push_front (*frag1);
        //     frame = frag0;
        //   }

        socket->SendTo(frame, 0, m_peer);
        m_bytesAddedToSocket += frame->GetSize();
        NS_LOG_INFO("Just sent " << frame->GetSerializedSize() << " " << frame->GetSize());
    }

    if (m_adaptationAlgorithmServer)
    {
        UintegerValue buf_size;

        Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(socket);
        socket->GetAttribute("SndBufSize", buf_size);
        if (buf_size.Get() == m_socket->GetTxAvailable() && m_txStarted != Seconds(0))
        {
            m_txTime += (Simulator::Now() - m_txStarted);
            m_txStarted = Seconds(0);
        }
    }
}

uint64_t
BurstyApplicationServerInstance::GetTotalTxBursts(void) const
{
    return m_totTxBursts;
}

uint64_t
BurstyApplicationServerInstance::GetTotalTxFragments(void) const
{
    return m_totTxFragments;
}

uint64_t
BurstyApplicationServerInstance::GetTotalTxBytes(void) const
{
    return m_totTxBytes;
}

} // Namespace ns3
