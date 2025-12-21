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

    for (uint32_t i = 1; i < averageBitrate.size(); i++)
    {
        if (averageBitrate[i] > result_non_quant)
        {
            return averageBitrate[i - 1];
        }
    }

    return averageBitrate[averageBitrate.size() - 1];
}

} // namespace ns3
