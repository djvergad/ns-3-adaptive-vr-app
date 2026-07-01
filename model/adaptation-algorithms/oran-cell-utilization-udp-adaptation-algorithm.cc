#include "oran-cell-utilization-udp-adaptation-algorithm.h"

#include "ns3/log.h"
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
                "Lcid",
                "Logical Channel ID for VR traffic bearer (typically 4-10 for dedicated bearers)",
                UintegerValue(5),
                MakeUintegerAccessor(&OranCellUtilizationUdpAdaptationAlgorithm::m_lcid),
                MakeUintegerChecker<uint8_t>(3, 32))
            .AddAttribute("UseDerivativeBitrate",
                          "If true, query GetVrBitrateDer instead of GetVrBitrate.",
                          BooleanValue(false),
                          MakeBooleanAccessor(
                              &OranCellUtilizationUdpAdaptationAlgorithm::m_useDerivativeBitrate),
                          MakeBooleanChecker())
            .AddAttribute("UseOptimizedBitrate",
                          "If true, query GetVrBitrateOptimized instead of GetVrBitrate.",
                          BooleanValue(false),
                          MakeBooleanAccessor(
                              &OranCellUtilizationUdpAdaptationAlgorithm::m_useOptimizedBitrate),
                          MakeBooleanChecker())
            .AddAttribute("UseDqnBitrate",
                          "If true, query GetVrBitrateDQN instead of GetVrBitrate.",
                          BooleanValue(false),
                          MakeBooleanAccessor(
                              &OranCellUtilizationUdpAdaptationAlgorithm::m_useDqnBitrate),
                          MakeBooleanChecker());
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

    // DEFENSIVE CHECK: Ensure m_lm is initialized and has valid data repository
    DataRate result_non_quant = DataRate(0);
    bool isDataValid = false;

    if (m_lm == nullptr)
    {
        NS_LOG_WARN("OranLogicVrBitrate not initialized, using ultra-conservative fallback rate");
        // Use extremely conservative fallback until initialized
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
            // Query the ORAN logic module for VR bitrate.
            if (m_useDerivativeBitrate)
            {
                result_non_quant = m_lm->GetVrBitrateDer(m_server_instance->m_peer, m_lcid);
            }
            else if (m_useOptimizedBitrate)
            {
                result_non_quant = m_lm->GetVrBitrateOptimized(m_server_instance->m_peer, m_lcid);
            }
            else if (m_useDqnBitrate)
            {
                result_non_quant = m_lm->GetVrBitrateDQN(m_server_instance->m_peer, m_lcid);
            }
            else
            {
                result_non_quant = m_lm->GetVrBitrate(m_server_instance->m_peer, m_lcid);
            }

            // DEFENSIVE CHECK: If GetVrBitrate returns 0 (data not available/initialized),
            // use ultra-conservative fallback to ensure absolutely no buffer overruns
            if (result_non_quant.GetBitRate() == 0)
            {
                NS_LOG_INFO("GetVrBitrate returned 0 (null/uninitialized data), using "
                            "ultra-conservative rate");
                // This conservative rate (500 kbps) ensures we never request more data than can fit
                // in the RLC buffer, even if fragment sizes are misunderstood
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

    NS_LOG_DEBUG("Requested bitrate from ORAN LM: " << result_non_quant.GetBitRate() / 1e6
                                                    << " Mbps "
                                                    << "(valid=" << isDataValid << ")");

    // return result_non_quant;

    std::vector<DataRate> averageBitrate = {55000,    77000,    108000,  151000,  212000,  297000,
                                            415000,   582000,   814000,  1140000, 1596000, 2234000,
                                            3128000,  3128000,  3254000, 3974000, 4496000, 6408000,
                                            10938000, 17156000, 35018000};

    // FIXED: Use ultra-conservative quantization - find the HIGHEST rung that is <= estimate
    // This prevents overshooting capacity which causes packet loss and RLC buffer overruns
    uint32_t chosenIdx = 0; // Default to lowest rung (55 kbps)
    for (uint32_t i = 0; i < averageBitrate.size(); i++)
    {
        if (averageBitrate[i] <= result_non_quant.GetBitRate())
        {
            chosenIdx = i; // Keep the highest rung we can afford
        }
        else
        {
            break; // Stop when we exceed the estimate
        }
    }

    DataRate chosenRate = averageBitrate[chosenIdx];
    NS_LOG_DEBUG("Selected bitrate rung: " << chosenRate.GetBitRate() / 1e6 << " Mbps (index "
                                           << chosenIdx << " of " << averageBitrate.size() - 1
                                           << ")");

    // DEFENSIVE CHECK: Ensure chosen rate is reasonable and bounded
    // Additional safety: cap at a reasonable maximum to prevent RLC buffer overflow
    const DataRate maxSafeRate = DataRate("40Mbps"); // Reduced from 50Mbps for extra safety
    if (chosenRate > maxSafeRate)
    {
        NS_LOG_WARN("Chosen rate " << chosenRate.GetBitRate() / 1e6
                                   << " Mbps exceeds safety limit, "
                                   << "capping at " << maxSafeRate.GetBitRate() / 1e6 << " Mbps");
        return maxSafeRate;
    }

    // ADDITIONAL SAFETY: When data is invalid, explicitly cap at the rung just above 500kbps
    // to ensure absolutely no aggressive transmission until ORAN is ready
    if (!isDataValid && chosenRate > DataRate("1Mbps"))
    {
        NS_LOG_WARN("Data is invalid, capping rate at 1 Mbps for safety");
        chosenRate = DataRate("1Mbps");
    }

    return chosenRate;
}

} // namespace ns3
