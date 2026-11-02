/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "gcc-algorithm-server.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("GccAlgorithmServer");
NS_OBJECT_ENSURE_REGISTERED(GccAlgorithmServer);

TypeId
GccAlgorithmServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::GccAlgorithmServer")
            .SetParent<AdaptationAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<GccAlgorithmServer>()
            .AddAttribute("OveruseThreshold",
                          "Initial delay-gradient threshold for classifying overuse.",
                          TimeValue(MilliSeconds(12.5)),
                          MakeTimeAccessor(&GccAlgorithmServer::m_overuseThreshold),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddAttribute("StartRate",
                          "Initial target rate; the stream ramps up from it based on feedback. "
                          "Capped at the application's configured rate.",
                          DataRateValue(DataRate("10Mbps")),
                          MakeDataRateAccessor(&GccAlgorithmServer::m_startRate),
                          MakeDataRateChecker())
            .AddAttribute("MinimumRate",
                          "Lower bound for the GCC target rate.",
                          DataRateValue(DataRate("10kbps")),
                          MakeDataRateAccessor(&GccAlgorithmServer::m_minRate),
                          MakeDataRateChecker())
            .AddAttribute("MaximumRate",
                          "Upper bound for the GCC target rate.",
                          DataRateValue(DataRate("100Mbps")),
                          MakeDataRateAccessor(&GccAlgorithmServer::m_maxRate),
                          MakeDataRateChecker())
            .AddAttribute("IncreaseFactor",
                          "Multiplicative increase factor when feedback indicates no congestion.",
                          DoubleValue(1.05),
                          MakeDoubleAccessor(&GccAlgorithmServer::m_increaseFactor),
                          MakeDoubleChecker<double>(1.0))
            .AddAttribute("DecreaseFactor",
                          "Multiplicative rate factor when delay indicates overuse.",
                          DoubleValue(0.85),
                          MakeDoubleAccessor(&GccAlgorithmServer::m_decreaseFactor),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("LossThreshold",
                          "Packet loss fraction below which the loss controller increases rate.",
                          DoubleValue(0.02),
                          MakeDoubleAccessor(&GccAlgorithmServer::m_lossThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("HighLossThreshold",
                          "Packet loss fraction above which the loss controller decreases rate.",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&GccAlgorithmServer::m_highLossThreshold),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("LossDecreaseFactor",
                          "Coefficient for GCC's loss-dependent rate decrease.",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&GccAlgorithmServer::m_lossDecreaseFactor),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

GccAlgorithmServer::GccAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

GccAlgorithmServer::~GccAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

void
GccAlgorithmServer::SetInitialRate(DataRate rate)
{
    AdaptationAlgorithmServer::SetInitialRate(rate);
    m_initialRate = std::max(m_minRate, std::min(rate, m_maxRate));
    const DataRate start = std::clamp(m_startRate, m_minRate, m_initialRate);
    m_delayRate = start;
    m_lossRate = start;
    m_targetRate = start;
    m_holdDelayRate = false;
    m_adaptiveThreshold = m_overuseThreshold;
}

void
GccAlgorithmServer::ReportFeedback(Time queueDelay,
                                   Time delayGradient,
                                   DataRate receiveRate,
                                   double lossFraction)
{
    AdaptationAlgorithmServer::ReportFeedback(queueDelay, delayGradient, receiveRate, lossFraction);

    const double sample = delayGradient.GetSeconds();
    m_filteredGradientSeconds =
        m_hasGradient ? 0.8 * m_filteredGradientSeconds + 0.2 * sample : sample;
    m_hasGradient = true;

    const double thresholdTarget =
        std::clamp(2.0 * std::abs(m_filteredGradientSeconds), 0.005, 0.050);
    const double thresholdSeconds = m_adaptiveThreshold.GetSeconds();
    if (std::abs(m_filteredGradientSeconds) <= thresholdSeconds)
    {
        m_adaptiveThreshold = Seconds(0.9 * thresholdSeconds + 0.1 * thresholdTarget);
    }

    if (m_filteredGradientSeconds > m_adaptiveThreshold.GetSeconds())
    {
        const uint64_t decreased =
            static_cast<uint64_t>(m_delayRate.GetBitRate() * m_decreaseFactor);
        m_delayRate = DataRate(std::max(m_minRate.GetBitRate(), decreased));
        if (receiveRate.GetBitRate() > 0)
        {
            m_delayRate = std::min(
                m_delayRate,
                DataRate(static_cast<uint64_t>(receiveRate.GetBitRate() * m_decreaseFactor)));
        }
    }
    else if (m_holdDelayRate)
    {
        // As in the GCC rate controller, hold for one report after a decrease.
    }
    else
    {
        uint64_t increased = static_cast<uint64_t>(m_delayRate.GetBitRate() * m_increaseFactor);
        if (receiveRate.GetBitRate() > 0)
        {
            // Do not probe far beyond what the receiver actually observes.
            increased = std::min<uint64_t>(increased,
                                           static_cast<uint64_t>(receiveRate.GetBitRate() * 1.5));
            increased = std::max(increased, m_delayRate.GetBitRate());
        }
        m_delayRate = DataRate(increased);
    }
    m_holdDelayRate = m_filteredGradientSeconds > m_adaptiveThreshold.GetSeconds();

    if (m_lossFraction < m_lossThreshold)
    {
        m_lossRate = DataRate(static_cast<uint64_t>(m_lossRate.GetBitRate() * m_increaseFactor));
    }
    else if (m_lossFraction > m_highLossThreshold)
    {
        const double decrease = 1.0 - m_lossDecreaseFactor * m_lossFraction;
        m_lossRate = DataRate(static_cast<uint64_t>(m_lossRate.GetBitRate() * decrease));
    }

    m_delayRate = std::clamp(m_delayRate, m_minRate, m_initialRate);
    m_lossRate = std::clamp(m_lossRate, m_minRate, m_initialRate);
    m_targetRate = std::min(m_delayRate, m_lossRate);

    NS_LOG_DEBUG("delayGradient=" << delayGradient.As(Time::MS)
                                  << "ms threshold=" << m_adaptiveThreshold.As(Time::MS)
                                  << "ms loss=" << m_lossFraction
                                  << " targetRate=" << m_targetRate.GetBitRate());
}

DataRate
GccAlgorithmServer::adaptation_algorithm(double, double, DataRate)
{
    return m_targetRate;
}

} // namespace ns3
