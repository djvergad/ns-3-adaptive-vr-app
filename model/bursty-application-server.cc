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
#include "bursty-application-server.h"

#include "ns3/address-utils.h"
#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/fuzzy-algorithm-server.h"
#include "ns3/bola.h"
#include "ns3/festive.h"
#include "ns3/mpc.h"
#include "ns3/google-algorithm-server.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-socket.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BurstyApplicationServer");

NS_OBJECT_ENSURE_REGISTERED(BurstyApplicationServer);

TypeId
BurstyApplicationServer::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::BurstyApplicationServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<BurstyApplicationServer>()
            .AddAttribute("Local",
                          "The Address on which to Bind the rx socket.",
                          AddressValue(),
                          MakeAddressAccessor(&BurstyApplicationServer::m_local),
                          MakeAddressChecker())
            .AddAttribute("Protocol",
                          "The type id of the protocol to use for the rx socket.",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&BurstyApplicationServer::m_tid),
                          MakeTypeIdChecker())
            .AddAttribute("MaxTargetDataRate",
                          "The max target datarate of the generator.",
                          DataRateValue(false),
                          MakeDataRateAccessor(&BurstyApplicationServer::m_maxTargetDataRate),
                          MakeTypeIdChecker())
            .AddAttribute("adaptationAlgorithm",
                          "The adaptation algorithm used, empty string for no adaptation. Other "
                          "allowed values are FuzzyAlgorithmServer and GoogleAlgorithmServer",
                          StringValue(""),
                          MakeStringAccessor(&BurstyApplicationServer::m_adaptationAlgorithm),
                          MakeStringChecker())
            .AddAttribute("FragmentSize",
                          "The size of packets sent in a burst including SeqTsSizeFragHeader",
                          UintegerValue(1200),
                          MakeUintegerAccessor(&BurstyApplicationServer::m_fragSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("appDuration",
                          "The amount of time each instance will generate packets",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&BurstyApplicationServer::m_appDuration),
                          MakeTimeChecker())

            .AddTraceSource("FragmentRx",
                            "A fragment has been received",
                            MakeTraceSourceAccessor(&BurstyApplicationServer::m_txFragmentTrace),
                            "ns3::BurstSink::SeqTsSizeFragCallback")
            .AddTraceSource("BurstRx",
                            "A burst has been successfully received",
                            MakeTraceSourceAccessor(&BurstyApplicationServer::m_txBurstTrace),
                            "ns3::BurstSink::SeqTsSizeFragCallback");
    return tid;
}

BurstyApplicationServer::BurstyApplicationServer()
{
    NS_LOG_FUNCTION(this);
}

BurstyApplicationServer::~BurstyApplicationServer()
{
    NS_LOG_FUNCTION(this);
}

uint64_t
BurstyApplicationServer::GetTotalTxBytes() const
{
    NS_LOG_FUNCTION(this);
    uint64_t totTxBytes = 0;
    for (const auto& kv : m_server_instances)
    {
        totTxBytes += kv.second.m_totTxBytes;
    }
    return totTxBytes;
}

uint64_t
BurstyApplicationServer::GetTotalTxFragments() const
{
    NS_LOG_FUNCTION(this);
    uint64_t totTxFragments = 0;
    for (const auto& kv : m_server_instances)
    {
        totTxFragments += kv.second.m_totTxFragments;
    }
    return totTxFragments;
}

uint64_t
BurstyApplicationServer::GetTotalTxBursts() const
{
    NS_LOG_FUNCTION(this);
    uint64_t totTxBursts = 0;
    for (const auto& kv : m_server_instances)
    {
        totTxBursts += kv.second.m_totTxBursts;
    }
    return totTxBursts;
}

Ptr<Socket>
BurstyApplicationServer::GetListeningSocket(void) const
{
    NS_LOG_FUNCTION(this);
    return m_socket;
}

std::list<Ptr<Socket>>
BurstyApplicationServer::GetAcceptedSockets(void) const
{
    NS_LOG_FUNCTION(this);
    return m_socketList;
}

std::map<Address, BurstyApplicationServerInstance>
BurstyApplicationServer::GetInstances(void) const
{
    NS_LOG_FUNCTION(this);
    return m_server_instances;
}

void
BurstyApplicationServer::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    m_socketList.clear();

    // chain up
    Application::DoDispose();
}

// Application Methods
void
BurstyApplicationServer::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        if (m_socket->Bind(m_local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Listen();
    }

    m_socket->SetRecvCallback(MakeCallback(&BurstyApplicationServer::HandleRead, this));
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                MakeCallback(&BurstyApplicationServer::HandleAccept, this));
    m_socket->SetCloseCallbacks(MakeCallback(&BurstyApplicationServer::HandleClose, this),
                                MakeCallback(&BurstyApplicationServer::HandleError, this));
}

void
BurstyApplicationServer::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);
    while (!m_socketList.empty()) // these are accepted sockets, close them
    {
        Ptr<Socket> acceptedSocket = m_socketList.front();
        Address peer;
        acceptedSocket->GetPeerName(peer);
        m_server_instances[peer].CancelEvents();
        m_socketList.pop_front();
        acceptedSocket->Close();
    }
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    for (auto& kv : m_server_instances)
    {
        kv.second.CancelEvents();
    }
}

void
BurstyApplicationServer::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    // std::cout << m_tid << std::endl;

    if ((m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
         m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET) ||
        m_tid.GetName() == "ns3::QuicSocketFactory")
    {
        Address peer;
        ;

        Ptr<Packet> packet = socket->RecvFrom(peer);

        CreateInstance(socket, peer);
    }
}

void
BurstyApplicationServer::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address peer;
    socket->GetPeerName(peer);

    m_server_instances[peer].CancelEvents();
}

void
BurstyApplicationServer::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address peer;
    socket->GetPeerName(peer);
    m_server_instances[peer].CancelEvents();

    // NS_ABORT_MSG ("Socket closed with error");
}

void
BurstyApplicationServer::HandleClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    for (auto& kv : m_server_instances)
    {
        kv.second.CancelEvents();
    }
}

void
BurstyApplicationServer::HandleError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    NS_ABORT_MSG("Socket closed with error");
}

void
BurstyApplicationServer::HandleAccept(Ptr<Socket> s, const Address& from)
{
    NS_LOG_FUNCTION(this << s << from);
    s->SetRecvCallback(MakeCallback(&BurstyApplicationServer::HandleRead, this));
    m_socketList.push_back(s);

    Address peer;
    s->GetPeerName(peer);

    s->SetSendCallback(MakeCallback(&BurstyApplicationServer::DataSend, this));
    s->SetDataSentCallback(MakeCallback(&BurstyApplicationServer::DataSend, this));
    s->SetCloseCallbacks(MakeCallback(&BurstyApplicationServer::HandlePeerClose, this),
                         MakeCallback(&BurstyApplicationServer::HandlePeerError, this));

    CreateInstance(s, peer);
}

void
BurstyApplicationServer::CreateInstance(Ptr<Socket> socket, Address peer)
{
    // m_server_instances.emplace (
    //     std::piecewise_construct, std::forward_as_tuple (peer),
    //     std::forward_as_tuple (socket, peer, m_txBurstTrace, m_txFragmentTrace));

    // m_server_instances[peer] = Create<BurstyApplicationServerInstance> ();
    m_server_instances[peer].m_socket = socket;

    m_server_instances[peer].m_peer = peer;
    m_server_instances[peer].m_txBurstTrace = m_txBurstTrace;
    m_server_instances[peer].m_txFragmentTrace = m_txFragmentTrace;
    m_server_instances[peer].m_fragSize = m_fragSize;

    if (m_adaptationAlgorithm == "FuzzyAlgorithmServer")
    {
        m_server_instances[peer].m_adaptationAlgorithmServer = CreateObject<FuzzyAlgorithmServer>();
    }
    else if (m_adaptationAlgorithm == "BolaAlgo")
    {
        m_server_instances[peer].m_adaptationAlgorithmServer = CreateObject<BolaAlgo>(0,0);
    }
    else if (m_adaptationAlgorithm == "MPCAlgo")
    {
        m_server_instances[peer].m_adaptationAlgorithmServer = CreateObject<MPCAlgo>(0,0);
    }
    else if (m_adaptationAlgorithm == "FestiveAlgorithm")
    {
        m_server_instances[peer].m_adaptationAlgorithmServer = CreateObject<FestiveAlgorithm>(0,0);
    }
    else if (m_adaptationAlgorithm == "GoogleAlgorithmServer")
    {
        m_server_instances[peer].m_adaptationAlgorithmServer =
            CreateObject<GoogleAlgorithmServer>();
    }
    else if (m_adaptationAlgorithm != "")
    {
        NS_ABORT_MSG("Wrong Adaptation Algorithm type");
    }

    Ptr<VrBurstGenerator> vrBurstGenerator =
        DynamicCast<VrBurstGenerator>(m_server_instances[peer].GetBurstGenerator());

    m_server_instances[peer].m_initRate = vrBurstGenerator->GetTargetDataRate();

    Simulator::Schedule(m_appDuration,
                        &BurstyApplicationServerInstance::StopBursts,
                        &m_server_instances[peer]);

    // vrBurstGenerator->SetFrameRate(60);
    // vrBurstGenerator->SetTargetDataRate(DataRate ("100Mbps"));
    // vrBurstGenerator->SetVrAppName( "VirusPopper");
    // m_server_instances[peer].GetBurstGenerator ()->SetAttribute ("FrameRate", DoubleValue (60));
    // m_server_instances[peer].GetBurstGenerator ()->SetAttribute ("TargetDataRate",
    //                                                              DataRateValue (DataRate
    //                                                              ("1Mbps")));
    // m_server_instances[peer].GetBurstGenerator ()->SetAttribute ("VrAppName",
    //                                                              StringValue ("VirusPopper"));

    // burstyHelper.SetBurstGenerator ("ns3::VrBurstGenerator",
    //                                 "FrameRate", DoubleValue (frameRate),
    //                                 "TargetDataRate", DataRateValue (DataRate (targetDataRate)),
    //                                 "VrAppName", StringValue(vrAppName));

    m_server_instances[peer].CancelEvents();
    m_server_instances[peer].SendBurst();
}

void
BurstyApplicationServer::DataSend(Ptr<Socket> socket, uint32_t num)
{
    Address peer;
    socket->GetPeerName(peer);

    m_server_instances[peer].DataSend(socket, num);
}

} // Namespace ns3
