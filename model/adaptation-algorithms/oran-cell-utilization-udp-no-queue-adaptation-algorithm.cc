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
            .AddAttribute("OranLogicVrBitrate",
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

OranCellUtilizationUdpNoQueueAdaptationAlgorithm::~OranCellUtilizationUdpNoQueueAdaptationAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

DataRate
OranCellUtilizationUdpNoQueueAdaptationAlgorithm::adaptation_algorithm(double buffOcc,
                                                                double diffBuffOcc,
                                                                DataRate lastRate)
{
    NS_LOG_FUNCTION(this << buffOcc << diffBuffOcc << lastRate);

    DataRate result_non_quant = m_lm->GetVrBitrateNoTxQueue(m_server_instance->m_peer, 5);

    std::vector<DataRate> averageBitrate = {55000,    77000,    108000,  151000,  212000,  297000,
                                            415000,   582000,   814000,  1140000, 1596000, 2234000,
                                            3128000,  3128000,  3254000, 3974000, 4496000, 6408000,
                                            10938000, 17156000, 35018000};

    // Choose the first rung that is greater than or equal to the estimate
    // This is more aggressive and helps ramp up to fill LTE TTI capacity
    uint32_t chosenIdx = averageBitrate.size() - 1;
    for (uint32_t i = 0; i < averageBitrate.size(); i++)
    {
        if (averageBitrate[i] >= result_non_quant)
        {
            chosenIdx = i;
            break;
        }
    }
    // If the estimate is close to the chosen rung, add headroom by stepping up one rung.
    // This helps overcome underestimation due to low offered load.
    if (chosenIdx < averageBitrate.size() - 1)
    {
        double threshold = 0.8 * static_cast<double>(averageBitrate[chosenIdx].GetBitRate());
        if (static_cast<double>(result_non_quant.GetBitRate()) > threshold)
        {
            chosenIdx = std::min(chosenIdx + 1, static_cast<uint32_t>(averageBitrate.size() - 1));
        }
    }
    return averageBitrate[chosenIdx];
}

} // namespace ns3
