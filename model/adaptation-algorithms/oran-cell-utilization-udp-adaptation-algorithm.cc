#include "oran-cell-utilization-udp-adaptation-algorithm.h"

#include "ns3/address.h"
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
            .AddAttribute("RicServer",
                          "Address (Ipv4 and port) of an OranVrBitrateServer. If set, bitrates are "
                          "requested over the network instead of via a direct call.",
                          AddressValue(),
                          MakeAddressAccessor(&OranCellUtilizationUdpAdaptationAlgorithm::m_ricServer),
                          MakeAddressChecker())
            .AddAttribute("OranLogicVrBitrate",
                          "The OranLogicVrBitrate used.",
                          PointerValue(0),
                          MakePointerAccessor(&OranCellUtilizationUdpAdaptationAlgorithm::m_lm),
                          MakePointerChecker<OranLogicVrBitrate>())
            .AddAttribute(
                "Lcid",
                "Logical Channel ID for VR traffic bearer (typically 4-10 for dedicated bearers)",
                UintegerValue(5),
                MakeUintegerAccessor(&OranCellUtilizationUdpAdaptationAlgorithm::m_lcid),
                MakeUintegerChecker<uint8_t>(3, 32));
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

    if (m_ricServer.IsInvalid() == false && m_server_instance != nullptr)
    {
        if (m_ricClient == nullptr)
        {
            m_ricClient = CreateObject<OranVrBitrateClient>();
            m_ricClient->Setup(m_server_instance->GetSocket()->GetNode(), m_ricServer);
        }
        // Asynchronous: ask the RIC for a fresh value and use the latest answer received
        m_ricClient->Request(m_server_instance->m_peer, m_lcid, false);
        result_non_quant = m_ricClient->GetLatestBitrate();
        if (result_non_quant.GetBitRate() == 0)
        {
            NS_LOG_INFO("No valid bitrate received from the RIC yet, using conservative rate");
            result_non_quant = DataRate("0.5Mbps");
        }
        else
        {
            isDataValid = true;
        }
    }
    else if (m_lm == nullptr)
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
            result_non_quant = m_lm->GetVrBitrate(m_server_instance->m_peer, m_lcid);

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

    return result_non_quant;
}

} // namespace ns3
