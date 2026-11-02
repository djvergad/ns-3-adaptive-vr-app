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
#include "bursty-application-client.h"

#include "ns3/address-utils.h"
#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/congestion-feedback-header.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-socket.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BurstyApplicationClient");

NS_OBJECT_ENSURE_REGISTERED(BurstyApplicationClient);

TypeId
BurstyApplicationClient::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::BurstyApplicationClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<BurstyApplicationClient>()
            .AddAttribute("Local",
                          "The Address on which to Bind the rx socket.",
                          AddressValue(),
                          MakeAddressAccessor(&BurstyApplicationClient::m_local),
                          MakeAddressChecker())

            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&BurstyApplicationClient::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Protocol",
                          "The type id of the protocol to use for the rx socket.",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&BurstyApplicationClient::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("RequestTimeout",
                          "The time to wait before retransmitting the initial UDP request",
                          TimeValue(Seconds(0.02)),
                          MakeTimeAccessor(&BurstyApplicationClient::m_requestTimeout),
                          MakeTimeChecker())
            .AddTraceSource("FragmentRx",
                            "A fragment has been received",
                            MakeTraceSourceAccessor(&BurstyApplicationClient::m_rxFragmentTrace),
                            "ns3::BurstSink::SeqTsSizeFragCallback")
            .AddTraceSource("BurstRx",
                            "A burst has been successfully received",
                            MakeTraceSourceAccessor(&BurstyApplicationClient::m_rxBurstTrace),
                            "ns3::BurstSink::SeqTsSizeFragCallback");
    return tid;
}

BurstyApplicationClient::BurstyApplicationClient()
{
    NS_LOG_FUNCTION(this);
}

BurstyApplicationClient::~BurstyApplicationClient()
{
    NS_LOG_FUNCTION(this);
}

uint64_t
BurstyApplicationClient::GetTotalRxBytes() const
{
    NS_LOG_FUNCTION(this);
    return m_totRxBytes;
}

uint64_t
BurstyApplicationClient::GetTotalRxFragments() const
{
    NS_LOG_FUNCTION(this);
    return m_totRxFragments;
}

uint64_t
BurstyApplicationClient::GetTotalRxBursts() const
{
    NS_LOG_FUNCTION(this);
    return m_totRxBursts;
}

void
BurstyApplicationClient::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    m_socket = 0;

    // chain up
    Application::DoDispose();
}

// Application Methods
void
BurstyApplicationClient::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        int ret = -1;

        if (!m_local.IsInvalid())
        {
            NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_peer) &&
                             InetSocketAddress::IsMatchingType(m_local)) ||
                                (InetSocketAddress::IsMatchingType(m_peer) &&
                                 Inet6SocketAddress::IsMatchingType(m_local)),
                            "Incompatible peer and local address IP version");
            ret = m_socket->Bind(m_local);
        }
        else
        {
            if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peer) ||
                     PacketSocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind();
            }
        }

        if (ret == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        NS_LOG_DEBUG("Trying to connect");
        int res = m_socket->Connect(m_peer);
        NS_LOG_DEBUG("After connect, " << res << " " << m_socket->GetErrno());

        // m_socket->SetAllowBroadcast (true);
        // m_socket->ShutdownRecv ();

        m_socket->SetConnectCallback(
            MakeCallback(&BurstyApplicationClient::ConnectionSucceeded, this),
            MakeCallback(&BurstyApplicationClient::ConnectionFailed, this));

        m_socket->SetCloseCallbacks(MakeCallback(&BurstyApplicationClient::HandlePeerClose, this),
                                    MakeCallback(&BurstyApplicationClient::HandlePeerError, this));

        m_socket->SetRecvCallback(MakeCallback(&BurstyApplicationClient::HandleRead, this));

        if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
            m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
        {
            SendUdpRequest();
        }
    }
}

void
BurstyApplicationClient::SendUdpRequest()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket || m_socket->GetTxAvailable() == 0)
    {
        NS_LOG_WARN("Socket not ready or Tx buffer unavailable. Postponing transmission.");
        return;
    }

    Ptr<Packet> dummy = Create<Packet>(100);
    m_socket->Send(dummy);
    NS_LOG_INFO("Sent UDP request, scheduling retransmission in " << m_requestTimeout.As(Time::S));

    // Schedule the next retransmission attempt
    m_requestEvent =
        Simulator::Schedule(m_requestTimeout, &BurstyApplicationClient::SendUdpRequest, this);
}

void
BurstyApplicationClient::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_requestEvent);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
BurstyApplicationClient::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_requestEvent.IsPending())
    {
        Simulator::Cancel(m_requestEvent);
        NS_LOG_INFO("Server response received. Canceling UDP request retransmissions.");
    }

    Ptr<Packet> fragment;
    Address from;
    Address localAddress;
    bool isStream = (socket->GetSocketType() == Socket::NS3_SOCK_STREAM ||
                     socket->GetSocketType() == Socket::NS3_SOCK_SEQPACKET);

    while ((fragment = socket->RecvFrom(from)))
    {
        if (fragment->GetSize() == 0)
        {
            break; // EOF
        }

        // --- PROTOCOL BRANCHING LAYER ---
        if (isStream)
        {
            // TCP: Reassemble byte-stream
            if (m_incomplete_packets.count(socket) == 1 && m_incomplete_packets[socket] != nullptr)
            {
                m_incomplete_packets[socket]->AddAtEnd(fragment);
            }
            else
            {
                m_incomplete_packets[socket] = fragment->Copy();
            }
        }
        else
        {
            // UDP: Every datagram is an independent unit. Do NOT append to previous states.
            m_incomplete_packets[socket] = fragment->Copy();
        }

        while (m_incomplete_packets[socket] && m_incomplete_packets[socket]->GetSize() > 0)
        {
            SeqTsSizeFragHeader header;

            if (m_incomplete_packets[socket]->GetSize() < header.GetSerializedSize())
            {
                if (!isStream)
                {
                    m_incomplete_packets[socket] = nullptr; // Discard corrupted UDP
                }
                break;
            }

            m_incomplete_packets[socket]->PeekHeader(header);

            if (header.GetFragBytes() == 0)
            {
                std::cout << "wrong header size " << std::endl;
                m_incomplete_packets[socket] = nullptr;
                break;
            }

            // --- CRITICAL SIZE VERIFICATION ---
            if (m_incomplete_packets[socket]->GetSize() < header.GetFragBytes())
            {
                if (isStream)
                {
                    // TCP: More data is coming down the stream. Wait for it.
                    break;
                }
                else
                {
                    // UDP: The datagram is truncated/corrupted. Discard it immediately.
                    NS_LOG_WARN("Corrupted UDP packet received: size smaller than header claims. "
                                "Discarding.");
                    m_incomplete_packets[socket] = nullptr;
                    break;
                }
            }

            int64_t extra_bytes = m_incomplete_packets[socket]->GetSize() - header.GetFragBytes();

            if (extra_bytes > 0)
            {
                fragment = m_incomplete_packets[socket]->CreateFragment(0, header.GetFragBytes());
                Ptr<Packet> frag2 =
                    m_incomplete_packets[socket]->CreateFragment(header.GetFragBytes(),
                                                                 extra_bytes);
                m_incomplete_packets[socket] = frag2;
            }
            else if (extra_bytes == 0)
            {
                fragment = m_incomplete_packets[socket]->Copy();
                m_incomplete_packets[socket] = nullptr;
            }
            if (extra_bytes < 0)
            {
                // m_incomplete_packet = m_incomplete_packet->Copy();
                SeqTsSizeFragHeader seqTs2;
                m_incomplete_packets[socket]->PeekHeader(seqTs2);

                // std::cout << "We have incompete m_incomplete_packet" << seqTs2.GetFragBytes()
                //           << std::endl;
                break;
            }

            m_totRxBytes += fragment->GetSize();

            std::stringstream addressStr;
            if (InetSocketAddress::IsMatchingType(from))
            {
                addressStr << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                           << InetSocketAddress::ConvertFrom(from).GetPort();
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                addressStr << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " port "
                           << Inet6SocketAddress::ConvertFrom(from).GetPort();
            }
            else
            {
                addressStr << "UNKNOWN ADDRESS TYPE";
            }

            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " burst sink received "
                                   << fragment->GetSize() << " bytes from " << addressStr.str()
                                   << " total Rx " << m_totRxBytes << " bytes");

            socket->GetSockName(localAddress);

            // handle received fragment
            auto itBuffer = m_burstHandlerMap.find(from); // rename m_burstBufferMap, itBuffer
            if (itBuffer == m_burstHandlerMap.end())
            {
                NS_LOG_LOGIC("New stream from " << from);
                itBuffer = m_burstHandlerMap.insert(std::make_pair(from, BurstHandler())).first;
            }

            if (header.GetSeq() != UINT32_MAX)
            {
                if (fragment->GetSize() == 0)
                {
                    break;
                }
                // std::cout << Simulator::Now() << " Received seq " << header.GetSeq() << " frag
                // seq " << header.GetFragSeq()<< " hsize " << header.GetSize() << " hfragbytes " <<
                // header.GetFragBytes() << " psize " << fragment->GetSize() << std::endl;

                FragmentReceived(itBuffer->second, fragment->Copy(), from, localAddress);
            }
            else
            {
                socket->Close();
            }
        }
    }
}

void
BurstyApplicationClient::FragmentReceived(BurstHandler& burstHandler,
                                          const Ptr<Packet>& f,
                                          const Address& from,
                                          const Address& localAddress)
{
    NS_LOG_FUNCTION(this << f);

    SeqTsSizeFragHeader header;
    f->PeekHeader(header);
    if (header.GetSize() == 0) {
        NS_LOG_UNCOND("Received fragment with header size 0. Discarding.");
        return;
    }

    if (header.GetFrags() == 0 || header.GetFragSeq() >= header.GetFrags())
    {
        NS_LOG_WARN("Received fragment with inconsistent header (fragSeq="
                    << header.GetFragSeq() << ", frags=" << header.GetFrags() << "). Discarding.");
        return;
    }

    ReportNetworkFeedback(header, f->GetSize());

    m_totRxFragments++;
    m_rxFragmentTrace(f,
                      m_peer,
                      localAddress,
                      header); // TODO should fragment still include header in trace?

    NS_LOG_DEBUG("Get BurstHandler for from="
                 << from << " with m_currentBurstSeq=" << burstHandler.m_currentBurstSeq
                 << ", m_fragmentsMerged=" << burstHandler.m_fragmentsMerged
                 << ", m_unorderedFragments.size ()=" << burstHandler.m_unorderedFragments.size()
                 << ", m_burstBuffer.GetSize ()=" << burstHandler.m_burstBuffer->GetSize()
                 << ", for fragment with header: " << header);

    if (header.GetSeq() < burstHandler.m_currentBurstSeq)
    {
        NS_LOG_LOGIC("Ignoring fragment from previous burst. Fragment burst seq="
                     << header.GetSeq()
                     << ", current burst seq=" << burstHandler.m_currentBurstSeq);
        return;
    }

    if (header.GetSeq() > burstHandler.m_currentBurstSeq)
    {
        // fragment of new burst: discard previous burst if incomplete
        NS_LOG_LOGIC("Start mering new burst seq "
                     << header.GetSeq() << " (previous=" << burstHandler.m_currentBurstSeq << ")");

        burstHandler.m_currentBurstSeq = header.GetSeq();
        burstHandler.m_fragmentsMerged = 0;
        burstHandler.m_unorderedFragments.clear();
        burstHandler.m_burstBuffer = Create<Packet>(0);
    }

    if (header.GetSeq() == burstHandler.m_currentBurstSeq)
    {
        // fragment of current burst
        NS_ASSERT_MSG(header.GetFragSeq() >= burstHandler.m_fragmentsMerged,
                      header.GetFragSeq() << " >= " << burstHandler.m_fragmentsMerged);

        NS_LOG_DEBUG("fragment sequence=" << header.GetFragSeq() << ", fragments merged="
                                          << burstHandler.m_fragmentsMerged);
        if (header.GetFragSeq() == burstHandler.m_fragmentsMerged)
        {
            // following packet: merge it
            f->RemoveHeader(header);
            burstHandler.m_burstBuffer->AddAtEnd(f);
            burstHandler.m_fragmentsMerged++;
            NS_LOG_LOGIC("Fragments merged " << burstHandler.m_fragmentsMerged << "/"
                                             << header.GetFrags() << " for burst "
                                             << header.GetSeq());

            // if present, merge following unordered fragments
            auto nextFragmentIt = burstHandler.m_unorderedFragments.begin();
            while (
                nextFragmentIt !=
                    burstHandler.m_unorderedFragments.end() && // there are unordered packets
                nextFragmentIt->first ==
                    burstHandler.m_fragmentsMerged) // the following fragment was already received
            {
                Ptr<Packet> storedFragment = nextFragmentIt->second;
                storedFragment->RemoveHeader(header);
                burstHandler.m_burstBuffer->AddAtEnd(storedFragment);
                burstHandler.m_fragmentsMerged++;
                NS_LOG_LOGIC("Unordered fragments merged " << burstHandler.m_fragmentsMerged << "/"
                                                           << header.GetFrags() << " for burst "
                                                           << header.GetSeq());

                nextFragmentIt = burstHandler.m_unorderedFragments.erase(nextFragmentIt);
            }
        }
        else
        {
            // add to unordered fragments buffer
            NS_LOG_LOGIC("Add unordered fragment " << header.GetFragSeq() << " of burst "
                                                   << header.GetSeq() << " to buffer ");
            burstHandler.m_unorderedFragments.insert(
                std::pair<uint16_t, const Ptr<Packet>>(header.GetFragSeq(), f));
        }
    }

    // check if burst is complete
    if (burstHandler.m_fragmentsMerged == header.GetFrags())
    {
        // all fragments have been merged
        NS_ASSERT_MSG(burstHandler.m_burstBuffer->GetSize() == header.GetSize(),
                      burstHandler.m_burstBuffer->GetSize() << " == " << header.GetSize());

        NS_LOG_LOGIC("Burst received: " << header.GetFrags() << " fragments for a total of "
                                        << header.GetSize() << " B " << header.GetSeq());
        m_totRxBursts++;
        m_rxBurstTrace(burstHandler.m_burstBuffer,
                       m_peer,
                       localAddress,
                       header); // TODO header size does not include payload, why?
    }
}

void
BurstyApplicationClient::ReportNetworkFeedback(const SeqTsSizeFragHeader& header,
                                               uint32_t packetSize)
{
    if (!m_socket || m_socket->GetSocketType() != Socket::NS3_SOCK_DGRAM)
    {
        return;
    }

    const Time now = Simulator::Now();
    const Time oneWayDelay = std::max(now - header.GetTs(), Seconds(0));
    if (!m_hasFeedbackSample || oneWayDelay < m_minimumOneWayDelay)
    {
        m_minimumOneWayDelay = oneWayDelay;
    }
    const Time queueDelay = oneWayDelay - m_minimumOneWayDelay;

    m_delayWindowSumSeconds += oneWayDelay.GetSeconds();
    m_delayWindowCount++;
    m_hasFeedbackSample = true;

    if (!m_hasFeedbackBurst || header.GetSeq() > m_feedbackBurstSequence)
    {
        if (m_hasFeedbackBurst && m_feedbackExpectedFragments > 0)
        {
            const double received =
                std::min<size_t>(m_feedbackReceivedFragments.size(), m_feedbackExpectedFragments);
            const double burstLoss =
                (m_feedbackExpectedFragments - received) / m_feedbackExpectedFragments;
            m_feedbackLossFraction = 0.75 * m_feedbackLossFraction + 0.25 * burstLoss;
        }
        m_feedbackBurstSequence = header.GetSeq();
        m_feedbackExpectedFragments = header.GetFrags();
        m_feedbackReceivedFragments.clear();
        m_hasFeedbackBurst = true;
    }
    if (header.GetSeq() == m_feedbackBurstSequence)
    {
        m_feedbackReceivedFragments.insert(header.GetFragSeq());
    }

    if (m_feedbackWindowStart == Seconds(0))
    {
        m_feedbackWindowStart = now;
    }
    m_feedbackWindowBytes += packetSize;
    const Time feedbackWindow = now - m_feedbackWindowStart;
    if (feedbackWindow >= m_feedbackInterval)
    {
        m_feedbackReceiveRate =
            DataRate(static_cast<uint64_t>(m_feedbackWindowBytes * 8.0 /
                                           feedbackWindow.GetSeconds()));
        m_feedbackWindowBytes = 0;
        m_feedbackWindowStart = now;
    }

    if (now < m_nextFeedbackTime)
    {
        return;
    }

    // Fragments of a burst arrive back to back, so per-fragment gradients are
    // meaningless; report the change in mean one-way delay between windows.
    Time delayGradient = Seconds(0);
    const double meanDelay = m_delayWindowSumSeconds / m_delayWindowCount;
    if (m_hasPreviousWindowDelay)
    {
        delayGradient = Seconds(meanDelay - m_previousWindowDelaySeconds);
    }
    m_previousWindowDelaySeconds = meanDelay;
    m_hasPreviousWindowDelay = true;
    m_delayWindowSumSeconds = 0.0;
    m_delayWindowCount = 0;

    CongestionFeedbackHeader feedback;
    feedback.SetMetrics(queueDelay,
                        delayGradient,
                        m_feedbackReceiveRate,
                        m_feedbackLossFraction);
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(feedback);
    const int sent = m_socket->Send(packet);
    if (sent < 0 || static_cast<uint32_t>(sent) != feedback.GetSerializedSize())
    {
        NS_LOG_WARN("Failed to send congestion feedback to the streaming server");
    }
    m_nextFeedbackTime = now + m_feedbackInterval;
}

void
BurstyApplicationClient::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
BurstyApplicationClient::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
BurstyApplicationClient::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
        m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
    {
        // Ptr<Packet> dummy = Create<Packet> (100);
        // m_socket->Send (dummy);
    }
}

void
BurstyApplicationClient::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}
} // Namespace ns3
