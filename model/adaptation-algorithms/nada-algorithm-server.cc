/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "nada-algorithm-server.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NadaAlgorithmServer");
NS_OBJECT_ENSURE_REGISTERED(NadaAlgorithmServer);

TypeId
NadaAlgorithmServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NadaAlgorithmServer")
            .SetParent<AdaptationAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<NadaAlgorithmServer>()
            .AddAttribute("StartRate",
                          "Initial reference rate; the stream ramps up from it based on feedback. "
                          "Capped at the application's configured rate.",
                          DataRateValue(DataRate("10Mbps")),
                          MakeDataRateAccessor(&NadaAlgorithmServer::m_startRate),
                          MakeDataRateChecker())
            .AddAttribute("MinimumRate",
                          "Lower bound for the NADA target rate.",
                          DataRateValue(DataRate("10kbps")),
                          MakeDataRateAccessor(&NadaAlgorithmServer::m_minRate),
                          MakeDataRateChecker())
            .AddAttribute("MaximumRate",
                          "Upper bound for the NADA target rate.",
                          DataRateValue(DataRate("100Mbps")),
                          MakeDataRateAccessor(&NadaAlgorithmServer::m_maxRate),
                          MakeDataRateChecker())
            .AddAttribute("QueueDelayThreshold",
                          "Queueing-delay threshold above which the delay signal is warped.",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_queueDelayThreshold),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddAttribute("LossPenalty",
                          "Delay-equivalent penalty applied to packet loss.",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_lossPenalty),
                          MakeTimeChecker())
            .AddAttribute("ReferenceQueueDelay",
                          "Equilibrium queueing delay used by the NADA rate controller.",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_referenceQueueDelay),
                          MakeTimeChecker())
            .AddAttribute("ReferenceLossRate",
                          "Reference packet loss fraction for the composite congestion signal.",
                          DoubleValue(0.02),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_referenceLossRate),
                          MakeDoubleChecker<double>(0.000001, 1.0))
            .AddAttribute("Priority",
                          "NADA flow priority weight.",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_priority),
                          MakeDoubleChecker<double>(0.000001))
            .AddAttribute("Gain",
                          "Gain applied to the congestion-signal offset.",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_gain),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("TrendGain",
                          "Gain applied to the change in congestion signal.",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_trendGain),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("DelayWarpFactor",
                          "Exponential delay-warping factor used above QueueDelayThreshold.",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_delayWarpFactor),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("IncreaseFactor",
                          "Multiplicative factor for the accelerated rate ramp-up.",
                          DoubleValue(1.05),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_increaseFactor),
                          MakeDoubleChecker<double>(1.0));
    return tid;
}

NadaAlgorithmServer::NadaAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

NadaAlgorithmServer::~NadaAlgorithmServer()
{
    NS_LOG_FUNCTION(this);
}

void
NadaAlgorithmServer::SetInitialRate(DataRate rate)
{
    AdaptationAlgorithmServer::SetInitialRate(rate);
    m_initialRate = std::max(m_minRate, std::min(rate, m_maxRate));
    m_targetRate = std::clamp(m_startRate, m_minRate, m_initialRate);
}

void
NadaAlgorithmServer::ReportFeedback(Time queueDelay,
                                    Time delayGradient,
                                    DataRate receiveRate,
                                    double lossFraction)
{
    AdaptationAlgorithmServer::ReportFeedback(queueDelay, delayGradient, receiveRate, lossFraction);

    const double delaySeconds = m_queueDelay.GetSeconds();
    const double thresholdSeconds = m_queueDelayThreshold.GetSeconds();
    double warpedDelay = delaySeconds;
    if (delaySeconds > thresholdSeconds)
    {
        warpedDelay =
            thresholdSeconds *
            std::exp(-m_delayWarpFactor * (delaySeconds - thresholdSeconds) / thresholdSeconds);
    }

    const double normalizedLoss = m_lossFraction / m_referenceLossRate;
    const double signalSeconds =
        warpedDelay + m_lossPenalty.GetSeconds() * normalizedLoss * normalizedLoss;
    const Time now = Simulator::Now();
    Time elapsed = now - m_lastFeedbackTime;
    if (elapsed <= Seconds(0))
    {
        elapsed = MilliSeconds(100);
    }
    elapsed = std::clamp(elapsed, MilliSeconds(10), MilliSeconds(500));

    const double currentRate = std::max(static_cast<double>(m_targetRate.GetBitRate()), 1.0);
    const double maxRate = static_cast<double>(m_initialRate.GetBitRate());
    const double equilibriumSignal =
        m_priority * m_referenceQueueDelay.GetSeconds() * maxRate / currentRate;
    const double signalDifference =
        m_hasPreviousSignal ? signalSeconds - m_previousSignalSeconds : 0.0;

    if (signalSeconds < equilibriumSignal && m_receiveRate.GetBitRate() > 0)
    {
        const double rampRate = m_receiveRate.GetBitRate() * m_increaseFactor;
        m_targetRate =
            std::max(m_targetRate, DataRate(static_cast<uint64_t>(std::min(rampRate, maxRate))));
    }
    else
    {
        const double rateChange =
            -m_gain * ((signalSeconds - equilibriumSignal) + m_trendGain * signalDifference) /
            elapsed.GetSeconds();
        const double boundedChange = std::clamp(rateChange, -0.25, 0.05);
        const double updatedRate = m_targetRate.GetBitRate() * (1.0 + boundedChange);
        m_targetRate = DataRate(static_cast<uint64_t>(updatedRate));
    }

    m_targetRate = std::clamp(m_targetRate, m_minRate, m_initialRate);
    m_previousSignalSeconds = signalSeconds;
    m_hasPreviousSignal = true;
    m_lastFeedbackTime = now;

    NS_LOG_DEBUG("queueDelay=" << m_queueDelay.As(Time::MS) << "ms loss=" << m_lossFraction
                               << " signal=" << signalSeconds
                               << "s targetRate=" << m_targetRate.GetBitRate());
}

DataRate
NadaAlgorithmServer::adaptation_algorithm(double, double, DataRate)
{
    return m_targetRate;
}

} // namespace ns3
