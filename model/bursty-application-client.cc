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
            SeqTsSizeFragHeader header;
            header.SetSeq(UINT32_MAX); // indicate dummy packet
            header.SetFrags(0);
            header.SetFragSeq(0);
            header.SetFragBytes(0);
            Ptr<Packet> request = Create<Packet>(100);
            request->AddHeader(header);
            m_socket->Send(request);

        } else {
            Simulator::Schedule(MilliSeconds(100), &BurstyApplicationClient::PeriodicTask, this);
        }
    }
}

void
BurstyApplicationClient::PeriodicTask()
{
    NS_LOG_FUNCTION(this);

    // Whatever you want to run every 100 ms
    NS_LOG_INFO("PeriodicTask executed at " << Simulator::Now().GetSeconds());


    if (m_socket && !m_finishing)
    {
        // Reschedule next execution
        Simulator::Schedule(MilliSeconds(100), &BurstyApplicationClient::PeriodicTask, this);

        Ptr<Packet> request = Create<Packet>(100);
        m_socket->Send(request);
        HandleRead(m_socket);
    }else if (m_finishing) {
        m_socket->ShutdownSend();
    }
}

void
BurstyApplicationClient::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);
    m_finishing = true;
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

    Ptr<Packet> packet;
    Address from;
    Address localAddress;

    SeqTsSizeFragHeader header;
    const uint32_t HEADER_SIZE = header.GetSerializedSize();

    NS_LOG_DEBUG("HandleRead called on socket");
    // Read all available packets and append to the reassembly buffer
    while ((packet = socket->RecvFrom(from)) && packet->GetSize() > 0)
    {
        NS_LOG_DEBUG("Packet received of size " << packet->GetSize());
        m_totRxBytes += packet->GetSize();

        if (packet->GetSize() == 0)
        {
            continue; // ignore empty
        }

        NS_LOG_DEBUG("Received packet of size " << packet->GetSize() << " from " << from);
        std::vector<uint8_t> tmp(packet->GetSize());
        packet->CopyData(tmp.data(), tmp.size());

        // append to per-socket buffer
        auto& buf = m_reassemblyBuffers[socket];
        buf.insert(buf.end(), tmp.begin(), tmp.end());
        NS_LOG_DEBUG("Reassembly buffer size is now " << buf.size());

        // try to parse as many complete fragments as possible
        // try to parse as many complete fragments as possible
        while (true)
        {
            // need header first
            if (buf.size() < HEADER_SIZE)
            {
                break; // wait for more bytes (partial header)
            }

            SeqTsSizeFragHeader hdr;
            // Parse only if we have full header bytes (ParseHeaderFromBuffer checks this)
            if (!ParseHeaderFromBuffer(buf, hdr))
            {
                // Not enough bytes for a full header (shouldn't happen because of the size check),
                // but be conservative: wait for more bytes.
                NS_LOG_DEBUG("Not enough bytes for full header yet — wait for more");
                break;
            }

            // total bytes we need for a full framed message = header + payload
            uint32_t totalNeeded = static_cast<uint32_t>(hdr.GetFragBytes());

            // Sanity check: protect against absurdly large fragBytes
            const uint32_t MAX_REASONABLE_FRAG = 10 * 1024 * 1024; // 10 MB
            if (hdr.GetFragBytes() > MAX_REASONABLE_FRAG)
            {
                NS_LOG_ERROR("Suspicious fragBytes=" << hdr.GetFragBytes()
                                                     << " — dropping connection data");
                buf.clear();
                break;
            }

            if (buf.size() < totalNeeded)
            {
                // not enough bytes for full fragment yet — wait for more
                NS_LOG_DEBUG("Not enough bytes for full fragment yet — wait for more");
                break;
            }

            // Extract payload (bytes after the header)
            std::vector<uint8_t> payload;
            if (hdr.GetFragBytes() > 0)
            {
                payload.insert(payload.end(), buf.begin() + HEADER_SIZE, buf.begin() + totalNeeded);
            }

            // Remove parsed bytes (header + payload) from buffer
            buf.erase(buf.begin(), buf.begin() + totalNeeded);

            // Now handle the fragment using the parsed 'hdr' (NOT 'header')
            // m_totRxBytes += hdr.GetFragBytes();

            // Build address string (same as before)...
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
                                   << totalNeeded << " bytes from " << addressStr.str()
                                   << " total Rx " << m_totRxBytes << " bytes");

            socket->GetSockName(localAddress);

            // handle received fragment
            auto itBuffer = m_burstHandlerMap.find(from);
            if (itBuffer == m_burstHandlerMap.end())
            {
                NS_LOG_LOGIC("New stream from " << from);
                itBuffer = m_burstHandlerMap.insert(std::make_pair(from, BurstHandler())).first;
            }

            if (hdr.GetSeq() != UINT32_MAX)
            {
                if (hdr.GetFragBytes() == 0)
                {
                    // zero-length fragment — nothing to push; continue
                    continue;
                }

                Ptr<Packet> fragment = Create<Packet>(payload.data(), payload.size());
                fragment->AddHeader(hdr);
                NS_LOG_DEBUG("Fragment received: seq="
                             << hdr.GetSeq() << " fragSeq=" << hdr.GetFragSeq()
                             << " fragBytes=" << hdr.GetFragBytes()
                             << " totalRxBytes=" << m_totRxBytes << " bufsize= " << buf.size());

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
    NS_ABORT_IF(header.GetSize() == 0);

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
