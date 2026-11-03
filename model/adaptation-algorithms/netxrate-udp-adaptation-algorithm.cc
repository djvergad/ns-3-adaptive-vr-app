/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "netxrate-udp-adaptation-algorithm.h"

#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NetXRateUdpAdaptationAlgorithm");
NS_OBJECT_ENSURE_REGISTERED(NetXRateUdpAdaptationAlgorithm);

TypeId
NetXRateUdpAdaptationAlgorithm::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NetXRateUdpAdaptationAlgorithm")
            .SetParent<AdaptationAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<NetXRateUdpAdaptationAlgorithm>()
            .AddAttribute("OranLogicVrBitrate",
                          "The shared ORAN logic instance providing cell radio measurements.",
                          PointerValue(0),
                          MakePointerAccessor(&NetXRateUdpAdaptationAlgorithm::m_lm),
                          MakePointerChecker<OranLogicVrBitrate>())
            .AddAttribute("Lcid",
                          "Logical channel ID for VR traffic.",
                          UintegerValue(5),
                          MakeUintegerAccessor(&NetXRateUdpAdaptationAlgorithm::m_lcid),
                          MakeUintegerChecker<uint8_t>(3, 32))
            .AddAttribute("RecommendationPeriod",
                          "Minimum period between NetXRate recommendations.",
                          TimeValue(MilliSeconds(500)),
                          MakeTimeAccessor(&NetXRateUdpAdaptationAlgorithm::m_recommendationPeriod),
                          MakeTimeChecker());
    return tid;
}

NetXRateUdpAdaptationAlgorithm::NetXRateUdpAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

NetXRateUdpAdaptationAlgorithm::~NetXRateUdpAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

void
NetXRateUdpAdaptationAlgorithm::SetInitialRate(DataRate rate)
{
    AdaptationAlgorithmServer::SetInitialRate(rate);
    m_initialRate = rate;
    m_cachedRecommendation = rate;
    m_hasRecommendation = false;
}

DataRate
NetXRateUdpAdaptationAlgorithm::adaptation_algorithm(double, double, DataRate)
{
    NS_LOG_FUNCTION(this);

    const DataRate fallback = std::min(m_initialRate, DataRate("100kbps"));
    if (m_lm == nullptr)
    {
        NS_LOG_WARN("OranLogicVrBitrate is not configured for NetXRate");
        return fallback;
    }
    if (m_server_instance == nullptr)
    {
        NS_LOG_WARN("BurstyApplicationServerInstance is not initialized for NetXRate");
        return fallback;
    }

    const Time now = Simulator::Now();
    if (m_hasRecommendation && now - m_lastRecommendationTime < m_recommendationPeriod)
    {
        return m_cachedRecommendation;
    }

    m_cachedRecommendation =
        std::min(m_initialRate,
                 m_lm->GetNetXRateRecommendation(m_server_instance->m_peer, m_lcid, m_initialRate));
    m_lastRecommendationTime = now;
    m_hasRecommendation = true;
    NS_LOG_DEBUG("NetXRate application cap: " << m_cachedRecommendation.GetBitRate() << " bps");
    return m_cachedRecommendation;
}

} // namespace ns3
