#include "oran-cell-utilization-udp-adaptation-algorithm.h"

#include "ns3/log.h"
#include "ns3/oran-cell-utilization-collector.h"
#include "ns3/pointer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranCellUtilizationUdpAdaptationAlgorithm");

NS_OBJECT_ENSURE_REGISTERED(OranCellUtilizationUdpAdaptationAlgorithm);

TypeId
OranCellUtilizationUdpAdaptationAlgorithm::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::OranCellUtilizationUdpAdaptationAlgorithm")
            .SetParent<AdaptationAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<OranCellUtilizationUdpAdaptationAlgorithm>()
            .AddAttribute(
                "OranLogicVrBitrate",
                "The OranLogicVrBitrate used.",
                PointerValue(0),
                MakePointerAccessor(&OranCellUtilizationUdpAdaptationAlgorithm::m_lm),
                MakePointerChecker<OranLogicVrBitrate>());
    return tid;
}

OranCellUtilizationUdpAdaptationAlgorithm::OranCellUtilizationUdpAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

OranCellUtilizationUdpAdaptationAlgorithm::~OranCellUtilizationUdpAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

// void
// OranCellUtilizationUdpAdaptationAlgorithm::SetOranLogicVrBitrate(
//     Ptr<OranLogicVrBitrate> lm)
// {
//     m_lm = lm;
// }

DataRate
OranCellUtilizationUdpAdaptationAlgorithm::adaptation_algorithm(double buffOcc,
                                                                double diffBuffOcc,
                                                                DataRate lastRate)
{
    NS_LOG_FUNCTION(this << buffOcc << diffBuffOcc << lastRate);


    return m_lm->GetVrBitrate(m_server_instance->m_peer, 5);


    // double utilization = 0.0;
    // double delay = 0.0;
    // double txQueueSize = 0.0;
    // if (m_collector)
    // {
    //     // Use the server instance peer address to query cell utilization. Works for UDP peers as
    //     // well because m_peer is an Address regardless of socket type.
    //     // utilization = m_collector->GetCurrentUtilizationByAddress(m_server_instance->m_peer);
    //     // delay = m_collector->DelayEstimate(0,0);
    //     // std::cout << "Estimated delay for " << m_server_instance->m_peer << ": " << delay << " s"
    //     // << std::endl;

    //     txQueueSize = m_collector->TxQueueSizeEstimate(0, 0);
    //     // std::cout << "Estimated Tx queue size for " << m_server_instance->m_peer << ": "
    //     //           << txQueueSize << " bytes" << std::endl;

    //     // if (m_txQueueSizeEma < 0.0)
    //     // {
    //     //     m_txQueueSizeEma = txQueueSize;
    //     // }

    //     // m_txQueueSizeEma = m_emaAlpha * txQueueSize + (1.0 - m_emaAlpha) * m_txQueueSizeEma;
    //     // NS_LOG_DEBUG("Smoothed utilization (EMA): " << m_txQueueSizeEma);

    //     // double upper = m_utilizationThreshold + m_hysteresisPercent;
    //     // double lower = m_utilizationThreshold - m_hysteresisPercent;

    //     // return m_maxRate;
    //     utilization = txQueueSize;
    // }
    // NS_LOG_DEBUG("Cell utilization: " << utilization);

    // // Initialize EMA on first run
    // if (m_utilizationEma < 0.0)
    // {
    //     m_utilizationEma = utilization;
    // }
    // // Update EMA to smooth utilization and avoid reacting to short spikes
    // m_utilizationEma = m_emaAlpha * utilization + (1.0 - m_emaAlpha) * m_utilizationEma;
    // NS_LOG_DEBUG("Smoothed utilization (EMA): " << m_utilizationEma);

    // double upper = m_utilizationThreshold + m_hysteresisPercent;
    // double lower = m_utilizationThreshold - m_hysteresisPercent;
    // Time now = Simulator::Now();

    // // std::cout << "Random " << m_random->GetValue(0.0, 1.0) << std::endl;
    // // If well above threshold -> multiplicative decrease (like TCP on loss)
    // if (m_utilizationEma > upper)
    // {
    //     if (now - m_lastAdaptationTimeDown < Seconds(0.5))
    //     {
    //         NS_LOG_DEBUG("Skipping adaptation to avoid too frequent changes");
    //         return m_prevRate;
    //     }
    //     m_lastAdaptationTimeDown = now;

    //     // Multiplicative decrease: cut the rate
    //     double factor = m_multiplicativeDecreaseFactor;
    //     double newBps = static_cast<double>(m_prevRate.GetBitRate()) * factor;
    //     DataRate newRate = DataRate(newBps);
    //     if (newRate < m_minRate)
    //     {
    //         newRate = m_minRate;
    //     }
    //     // Set slow-start threshold similar to TCP: ssthresh = newRate
    //     m_ssthresh = newRate;
    //     m_inSlowStart = false;
    //     NS_LOG_DEBUG("Multiplicative decrease (AIMD): factor=" << factor << " newRate=" << newRate);
    //     m_prevRate = newRate;
    //     return m_prevRate;
    // }

    // // If well below threshold -> increase rate (AIMD). Use slow-start if enabled.
    // if (m_utilizationEma < lower)
    // {
    //     if (now - m_lastAdaptationTimeUp < m_minAdaptInterval)
    //     {
    //         NS_LOG_DEBUG("Skipping adaptation to avoid too frequent changes");
    //         return m_prevRate;
    //     }
    //     m_lastAdaptationTimeUp = now;

    //     DataRate newRate = m_prevRate;
    //     if (m_slowStartEnabled && m_inSlowStart)
    //     {
    //         // Exponential growth: double the rate each step (bounded)
    //         double newBps = static_cast<double>(m_prevRate.GetBitRate()) * m_slowStartMultiplier;
    //         newRate = DataRate(newBps);
    //         if (newRate >= m_ssthresh)
    //         {
    //             // Exit slow start into congestion avoidance
    //             m_inSlowStart = false;
    //             // Cap at ssthresh
    //             newRate = m_ssthresh;
    //         }
    //         NS_LOG_DEBUG("Slow start increase to " << newRate << " (ssthresh=" << m_ssthresh
    //                                                << ")");
    //     }
    //     else
    //     {
    //         // Congestion avoidance (additive increase)
    //         double newBps = static_cast<double>(m_prevRate.GetBitRate()) +
    //                         static_cast<double>(m_additiveIncrease.GetBitRate());
    //         newRate = DataRate(newBps);
    //         NS_LOG_DEBUG("Additive increase: +" << m_additiveIncrease << " -> " << newRate);
    //     }

    //     if (newRate > m_maxRate)
    //     {
    //         newRate = m_maxRate;
    //     }
    //     m_prevRate = newRate;
    //     return m_prevRate;
    // }

    // // Within hysteresis band -> keep previous rate (stabilize)
    // NS_LOG_DEBUG("Within hysteresis band; no rate change");
    // return m_prevRate;
}

} // namespace ns3
