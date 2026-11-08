/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "gcc-ric-cap-algorithm-server.h"

#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("GccRicCapAlgorithmServer");
NS_OBJECT_ENSURE_REGISTERED(GccRicCapAlgorithmServer);

TypeId
GccRicCapAlgorithmServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::GccRicCapAlgorithmServer")
            .SetParent<GccAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<GccRicCapAlgorithmServer>()
            .AddAttribute("RicServer",
                          "Address (Ipv4 and port) of the OranVrBitrateServer that provides the "
                          "cap. If unset, the controller is plain GCC.",
                          AddressValue(),
                          MakeAddressAccessor(&GccRicCapAlgorithmServer::m_ricServer),
                          MakeAddressChecker())
            .AddAttribute("Lcid",
                          "Logical channel ID of the VR traffic, sent to the RIC.",
                          UintegerValue(5),
                          MakeUintegerAccessor(&GccRicCapAlgorithmServer::m_lcid),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("CapSource",
                          "Bitrate of the RIC used as the cap: the fair share alone, the fair "
                          "share with the queue back-off, or the NetXRate recommendation.",
                          EnumValue(OranVrBitrateServer::NO_QUEUE),
                          MakeEnumAccessor<OranVrBitrateServer::RateMode>(
                              &GccRicCapAlgorithmServer::m_capSource),
                          MakeEnumChecker(OranVrBitrateServer::NO_QUEUE,
                                          "FairShare",
                                          OranVrBitrateServer::QUEUE_AWARE,
                                          "QueueAware",
                                          OranVrBitrateServer::NETXRATE,
                                          "NetXRate"));
    return tid;
}

GccRicCapAlgorithmServer::GccRicCapAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

GccRicCapAlgorithmServer::~GccRicCapAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

void
GccRicCapAlgorithmServer::DoDispose()
{
    m_ricClient = nullptr;
    m_server_instance = nullptr;
    GccAlgorithmServer::DoDispose();
}

DataRate
GccRicCapAlgorithmServer::GetCap() const
{
    return m_ricClient ? m_ricClient->GetLatestBitrate() : DataRate(0);
}

void
GccRicCapAlgorithmServer::ApplyCap()
{
    const DataRate cap = GetCap();
    if (cap.GetBitRate() == 0)
    {
        return;
    }
    m_delayRate = std::min(m_delayRate, cap);
    m_lossRate = std::min(m_lossRate, cap);
    m_targetRate = std::min(m_delayRate, m_lossRate);
}

void
GccRicCapAlgorithmServer::ReportFeedback(Time queueDelay,
                                         Time delayGradient,
                                         DataRate receiveRate,
                                         double lossFraction)
{
    GccAlgorithmServer::ReportFeedback(queueDelay, delayGradient, receiveRate, lossFraction);
    ApplyCap();
}

DataRate
GccRicCapAlgorithmServer::adaptation_algorithm(double buffOcc, double diffBuffOcc, DataRate lastRate)
{
    if (!m_ricServer.IsInvalid() && m_server_instance != nullptr)
    {
        if (!m_ricClient)
        {
            m_ricClient = CreateObject<OranVrBitrateClient>();
            m_ricClient->Setup(m_server_instance->GetSocket()->GetNode(), m_ricServer);
        }
        // Asynchronous: the answer is used from the next decision on
        m_ricClient->Request(m_server_instance->m_peer, m_lcid, m_capSource);
    }
    ApplyCap();
    NS_LOG_DEBUG("GCC target " << m_targetRate.GetBitRate() / 1e6 << " Mbps, cap "
                               << GetCap().GetBitRate() / 1e6 << " Mbps");
    return GccAlgorithmServer::adaptation_algorithm(buffOcc, diffBuffOcc, lastRate);
}

} // namespace ns3
