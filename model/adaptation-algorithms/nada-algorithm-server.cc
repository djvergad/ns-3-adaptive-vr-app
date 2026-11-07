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
#include <array>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NadaAlgorithmServer");
NS_OBJECT_ENSURE_REGISTERED(NadaAlgorithmServer);

namespace
{
/// Weights of the loss intervals, newest first (RFC 5348, Section 5.4).
constexpr std::array<double, 8> LOSS_INTERVAL_WEIGHTS{1.0, 1.0, 1.0, 1.0, 0.8, 0.6, 0.4, 0.2};
} // namespace

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
                          "Queueing-delay threshold above which the delay signal is warped after "
                          "a recent loss (QTH in RFC 8698).",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_queueDelayThreshold),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddAttribute("LossPenalty",
                          "Delay-equivalent penalty at the reference loss rate (DLOSS in RFC 8698).",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_lossPenalty),
                          MakeTimeChecker())
            .AddAttribute("ReferenceQueueDelay",
                          "Equilibrium queueing delay used by the NADA rate controller "
                          "(XREF in RFC 8698).",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_referenceQueueDelay),
                          MakeTimeChecker())
            .AddAttribute("ReferenceLossRate",
                          "Reference packet loss fraction for the composite congestion signal "
                          "(PLRREF in RFC 8698).",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_referenceLossRate),
                          MakeDoubleChecker<double>(0.000001, 1.0))
            .AddAttribute("Priority",
                          "NADA flow priority weight (PRIO in RFC 8698).",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_priority),
                          MakeDoubleChecker<double>(0.000001))
            .AddAttribute("Gain",
                          "Gain applied to the congestion-signal offset (KAPPA in RFC 8698).",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_gain),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("TrendGain",
                          "Gain applied to the change in congestion signal (ETA in RFC 8698).",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_trendGain),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("DelayWarpFactor",
                          "Exponential delay-warping factor used above QueueDelayThreshold "
                          "(LAMBDA in RFC 8698).",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_delayWarpFactor),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("TimeConstant",
                          "Time constant of the gradual rate update (TAU in RFC 8698).",
                          TimeValue(MilliSeconds(500)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_timeConstant),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddAttribute("FeedbackInterval",
                          "Target feedback interval (DELTA in RFC 8698).",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_feedbackInterval),
                          MakeTimeChecker(MilliSeconds(1)))
            .AddAttribute("DelayFilterDelay",
                          "Latency of the delay filter (DFILT in RFC 8698).",
                          TimeValue(MilliSeconds(120)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_delayFilterDelay),
                          MakeTimeChecker())
            .AddAttribute("RampUpDelayBound",
                          "Bound on self-inflicted queueing delay during ramp-up "
                          "(QBOUND in RFC 8698).",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_rampDelayBound),
                          MakeTimeChecker())
            .AddAttribute("RampUpDelayThreshold",
                          "Queueing delay that counts as buildup and blocks ramp-up "
                          "(QEPS in RFC 8698).",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_rampDelayThreshold),
                          MakeTimeChecker())
            .AddAttribute("ObservationWindow",
                          "Window free of loss and delay buildup required for ramp-up "
                          "(LOGWIN in RFC 8698).",
                          TimeValue(MilliSeconds(500)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_observationWindow),
                          MakeTimeChecker())
            .AddAttribute("RoundTripTime",
                          "Round-trip time used by the ramp-up multiplier (rtt in RFC 8698). "
                          "Configured, because the feedback report carries no timestamp echo.",
                          TimeValue(MilliSeconds(20)),
                          MakeTimeAccessor(&NadaAlgorithmServer::m_roundTripTime),
                          MakeTimeChecker())
            .AddAttribute("MaximumRampUpGain",
                          "Upper bound of the ramp-up multiplier (GAMMA_MAX in RFC 8698).",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_gammaMax),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("LossExpiryMultiplier",
                          "Multiple of the average loss interval during which the delay stays "
                          "warped after a loss (MULTILOSS in RFC 8698).",
                          DoubleValue(7.0),
                          MakeDoubleAccessor(&NadaAlgorithmServer::m_lossExpiryMultiplier),
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
NadaAlgorithmServer::UpdateLossHistory(Time now)
{
    if (m_lossFraction > m_previousLossFraction)
    {
        const Time since = m_hasLoss ? now - m_lastLossTime : now - m_firstFeedbackTime;
        m_lossIntervals.push_front(std::max(since, m_feedbackInterval));
        if (m_lossIntervals.size() > LOSS_INTERVAL_WEIGHTS.size())
        {
            m_lossIntervals.pop_back();
        }
        m_lastLossTime = now;
        m_hasLoss = true;
    }
    m_previousLossFraction = m_lossFraction;
}

Time
NadaAlgorithmServer::GetAverageLossInterval() const
{
    double weightedSum = 0.0;
    double totalWeight = 0.0;
    for (size_t i = 0; i < m_lossIntervals.size(); ++i)
    {
        weightedSum += LOSS_INTERVAL_WEIGHTS[i] * m_lossIntervals[i].GetSeconds();
        totalWeight += LOSS_INTERVAL_WEIGHTS[i];
    }
    return Seconds(weightedSum / totalWeight);
}

double
NadaAlgorithmServer::WarpDelay(Time now, double delaySeconds) const
{
    const double thresholdSeconds = m_queueDelayThreshold.GetSeconds();
    if (!m_hasLoss || delaySeconds < thresholdSeconds)
    {
        return delaySeconds;
    }

    const double lossInterval = GetAverageLossInterval().GetSeconds();
    const double expiry = m_lossExpiryMultiplier * lossInterval;
    const double age = (now - m_lastLossTime).GetSeconds();
    if (age >= expiry)
    {
        return delaySeconds;
    }

    const double warped =
        thresholdSeconds *
        std::exp(-m_delayWarpFactor * (delaySeconds - thresholdSeconds) / thresholdSeconds);
    const double warpWeight = std::min(1.0, (expiry - age) / lossInterval);
    return warpWeight * warped + (1.0 - warpWeight) * delaySeconds;
}

bool
NadaAlgorithmServer::CanRampUp(Time now) const
{
    const bool recentLoss = m_hasLoss && now - m_lastLossTime < m_observationWindow;
    const bool recentBuildup =
        m_hasDelayBuildup && now - m_lastDelayBuildupTime < m_observationWindow;
    return !recentLoss && !recentBuildup;
}

void
NadaAlgorithmServer::ReportFeedback(Time queueDelay,
                                    Time delayGradient,
                                    DataRate receiveRate,
                                    double lossFraction)
{
    AdaptationAlgorithmServer::ReportFeedback(queueDelay, delayGradient, receiveRate, lossFraction);

    const Time now = Simulator::Now();
    if (!m_hasFeedback)
    {
        m_firstFeedbackTime = now;
    }
    const double delta =
        m_hasFeedback ? (now - m_lastFeedbackTime).GetSeconds() : m_feedbackInterval.GetSeconds();

    UpdateLossHistory(now);
    if (m_queueDelay >= m_rampDelayThreshold)
    {
        m_lastDelayBuildupTime = now;
        m_hasDelayBuildup = true;
    }

    // Aggregate congestion signal (Eq. 2)
    const double delayTilde = WarpDelay(now, m_queueDelay.GetSeconds());
    const double normalizedLoss = m_lossFraction / m_referenceLossRate;
    const double signalSeconds =
        delayTilde + m_lossPenalty.GetSeconds() * normalizedLoss * normalizedLoss;

    const double currentRate = std::max(static_cast<double>(m_targetRate.GetBitRate()), 1.0);
    const double maxRate = static_cast<double>(m_initialRate.GetBitRate());
    double updatedRate = currentRate;
    const bool rampUp = CanRampUp(now);
    if (rampUp)
    {
        // Accelerated ramp-up (Eqs. 3 and 4)
        const double gamma = std::min(m_gammaMax,
                                      m_rampDelayBound.GetSeconds() /
                                          (m_roundTripTime + m_feedbackInterval + m_delayFilterDelay)
                                              .GetSeconds());
        updatedRate = std::max(currentRate, (1.0 + gamma) * m_receiveRate.GetBitRate());
    }
    else
    {
        // Gradual update (Eqs. 5 to 7)
        const double tau = m_timeConstant.GetSeconds();
        const double offset =
            signalSeconds - m_priority * m_referenceQueueDelay.GetSeconds() * maxRate / currentRate;
        const double difference = signalSeconds - m_previousSignalSeconds;
        updatedRate = currentRate - m_gain * (delta / tau) * (offset / tau) * currentRate -
                      m_gain * m_trendGain * (difference / tau) * currentRate;
    }

    // Clip in floating point: Eq. 7 can go negative after a large jump of the signal (Eqs. 8, 9)
    updatedRate = std::clamp(updatedRate,
                             static_cast<double>(m_minRate.GetBitRate()),
                             std::max(maxRate, static_cast<double>(m_minRate.GetBitRate())));
    m_targetRate = DataRate(static_cast<uint64_t>(updatedRate));

    m_previousSignalSeconds = signalSeconds;
    m_lastFeedbackTime = now;
    m_hasFeedback = true;

    NS_LOG_DEBUG("queueDelay=" << m_queueDelay.As(Time::MS) << " loss=" << m_lossFraction
                               << " signal=" << signalSeconds << "s rampUp=" << rampUp
                               << " targetRate=" << m_targetRate.GetBitRate());
}

DataRate
NadaAlgorithmServer::adaptation_algorithm(double, double, DataRate)
{
    return m_targetRate;
}

} // namespace ns3
