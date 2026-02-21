#include "oran-cell-utilization-udp-no-queue-adaptation-algorithm.h"

#include "ns3/log.h"
#include "ns3/pointer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranCellUtilizationUdpNoQueueAdaptationAlgorithm");

NS_OBJECT_ENSURE_REGISTERED(OranCellUtilizationUdpNoQueueAdaptationAlgorithm);

TypeId
OranCellUtilizationUdpNoQueueAdaptationAlgorithm::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm")
            .SetParent<AdaptationAlgorithmServer>()
            .SetGroupName("Applications")
            .AddConstructor<OranCellUtilizationUdpNoQueueAdaptationAlgorithm>()
            .AddAttribute(
                "OranLogicVrBitrate",
                "The OranLogicVrBitrate used.",
                PointerValue(0),
                MakePointerAccessor(&OranCellUtilizationUdpNoQueueAdaptationAlgorithm::m_lm),
                MakePointerChecker<OranLogicVrBitrate>());
    return tid;
}

OranCellUtilizationUdpNoQueueAdaptationAlgorithm::OranCellUtilizationUdpNoQueueAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

OranCellUtilizationUdpNoQueueAdaptationAlgorithm::
    ~OranCellUtilizationUdpNoQueueAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

DataRate
OranCellUtilizationUdpNoQueueAdaptationAlgorithm::adaptation_algorithm(double buffOcc,
                                                                       double diffBuffOcc,
                                                                       DataRate lastRate)
{
    NS_LOG_FUNCTION(this << buffOcc << diffBuffOcc << lastRate);

    // DEFENSIVE CHECK: Ensure m_lm is initialized and has valid data repository
    DataRate result_non_quant = DataRate(0);
    bool isDataValid = false;

    if (m_lm == nullptr)
    {
        NS_LOG_WARN("OranLogicVrBitrate not initialized, using ultra-conservative fallback rate");
        result_non_quant = DataRate("0.5Mbps");
    }
    else
    {
        // DEFENSIVE CHECK: Ensure m_server_instance is valid before accessing m_peer
        if (m_server_instance == nullptr)
        {
            NS_LOG_WARN("BurstyApplicationServerInstance not initialized");
            result_non_quant = DataRate("0.5Mbps");
        }
        else
        {
            // Query the ORAN logic module for VR bitrate (without tx queue)
            result_non_quant = m_lm->GetVrBitrateNoTxQueue(m_server_instance->m_peer, 5);

            // DEFENSIVE CHECK: If GetVrBitrateNoTxQueue returns 0 (data not available/initialized),
            // use ultra-conservative fallback to ensure absolutely no buffer overruns
            if (result_non_quant.GetBitRate() == 0)
            {
                NS_LOG_INFO("GetVrBitrateNoTxQueue returned 0 (null/uninitialized data), using "
                            "ultra-conservative rate");
                // Use 500 kbps - extremely conservative to prevent RLC buffer issues
                result_non_quant = DataRate("0.5Mbps");
            }
            else
            {
                isDataValid = true;
                NS_LOG_DEBUG("Valid bitrate data from ORAN: " << result_non_quant.GetBitRate() / 1e6
                                                              << " Mbps");
            }
        }
    }

    NS_LOG_DEBUG("Requested bitrate from ORAN LM (no-queue): "
                 << result_non_quant.GetBitRate() / 1e6 << " Mbps (valid=" << isDataValid << ")");

    std::vector<DataRate> averageBitrate = {55000,    77000,    108000,  151000,  212000,  297000,
                                            415000,   582000,   814000,  1140000, 1596000, 2234000,
                                            3128000,  3128000,  3254000, 3974000, 4496000, 6408000,
                                            10938000, 17156000, 35018000};

    // MODIFIED: Use CONSERVATIVE quantization to prevent buffer overruns
    // Find the HIGHEST rung that is <= estimate (do NOT overshoot)
    uint32_t chosenIdx = 0; // Default to lowest rung (55 kbps)
    for (uint32_t i = 0; i < averageBitrate.size(); i++)
    {
        if (averageBitrate[i] <= result_non_quant.GetBitRate())
        {
            chosenIdx = i; // Keep the highest rung we can afford
        }
        else
        {
            break; // Stop when we would exceed the estimate
        }
    }

    DataRate chosenRate = averageBitrate[chosenIdx];
    NS_LOG_DEBUG("Selected bitrate rung: " << chosenRate.GetBitRate() / 1e6 << " Mbps (index "
                                           << chosenIdx << " of " << averageBitrate.size() - 1
                                           << ")");

    // DEFENSIVE CHECK: Ensure chosen rate is reasonable and bounded
    // Additional safety: cap at a reasonable maximum to prevent RLC buffer overflow
    const DataRate maxSafeRate = DataRate("40Mbps");
    if (chosenRate > maxSafeRate)
    {
        NS_LOG_WARN("Chosen rate " << chosenRate.GetBitRate() / 1e6
                                   << " Mbps exceeds safety limit, "
                                   << "capping at " << maxSafeRate.GetBitRate() / 1e6 << " Mbps");
        return maxSafeRate;
    }

    // ADDITIONAL SAFETY: When data is invalid, explicitly cap at ultra-conservative rate
    if (!isDataValid && chosenRate > DataRate("0.5Mbps"))
    {
        NS_LOG_WARN("Data is invalid, capping rate at 0.5 Mbps for safety (no-queue variant)");
        chosenRate = DataRate("0.5Mbps");
    }

    return chosenRate;
}

} // namespace ns3
