// SPDX-License-Identifier: GPL-2.0-only

#ifndef VR_NR_ORAN_COMMON_H
#define VR_NR_ORAN_COMMON_H

/**
 * @file vr-nr-oran-common.h
 * @brief Building blocks shared by the VR over NR with O-RAN examples.
 *
 * Used by the scenarios in vr-cttc-nr-oran-scenario.h and vr-nr-oran-handover-scenario.h.
 * Everything that is not specific to a scenario (VR application, QoS flow, transport, core
 * network delays, O-RAN reporting and RIC configuration) is configured here, so that all the
 * scenarios use the same settings. The functions are header-only so that the examples can
 * also be built as single-file scratch programs (scratch/ needs links to the headers).
 */

#include "ns3/applications-module.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/oran-e2-network-transport.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/oran-module.h"
#include "ns3/oran-reporter-nr-ue-stats.h"
#include "ns3/oran-vr-bitrate-service.h"
#include "ns3/point-to-point-module.h"

#include <cstdio>
#include <iostream>
#include <map>
#include <string>

namespace ns3
{
namespace vrnr
{

/// UDP/TCP port of the VR server.
const uint16_t VR_PORT = 1234;
/// Size of the VR fragments [bytes].
const uint32_t VR_FRAGMENT_SIZE = 1200;
/// LCID of the VR QoS flow. In the NR module the LCID is the DRB id; DRB ids 0, 1, 2 and 4
/// are not allocated, so the default DRB has LCID 3 and the first dedicated DRB LCID 5.
const uint8_t VR_LCID = 5;
/// UDP port of the RIC VR bitrate service (OranVrBitrateServer).
const uint16_t RIC_BITRATE_PORT = 9000;
/// Registration interval of the E2 terminators.
const Time E2_REGISTRATION_INTERVAL = MilliSeconds(500);
/// Time at which the RIC and the UE E2 terminators are activated.
const Time ORAN_START = Seconds(1);
/// Time at which the gNB E2 terminators are activated.
const Time ORAN_GNB_START = MilliSeconds(1500);

/**
 * VR application, transport and O-RAN options common to all the scenarios.
 *
 * The scenarios register them with AddCommandLineArgs(); the defaults are the same for all.
 */
struct VrOranOptions
{
    // VR application
    std::string appRate{"50Mbps"};           ///< VR application target data rate
    double frameRate{60};                    ///< VR application frame rate [FPS]
    std::string vrAppName{"VirusPopper"};    ///< VR application name (VrBurstGenerator model)
    std::string burstGeneratorType{"model"}; ///< Adaptation algorithm, see ConfigureBurstGenerator
    std::string vrQosFlow{"DGBR_VISUAL_CONTENT_89"}; ///< 5QI of the VR QoS flow
    std::string tcpType{"ns3::TcpBbr"};              ///< TCP variant of the TCP-based algorithms
    /// Discard the packets that wait at the gNB longer than the packet delay budget of their
    /// QoS flow (NrRlcUm PDCP discarding). Disabled by default, so that late packets show up
    /// as delay rather than as losses.
    bool pdbDiscard{false};

    // Radio and core network
    double ueTxPower{23};                  ///< UE TX power [dBm]
    Time remoteHostDelay{MilliSeconds(5)}; ///< Delay of the PGW - VR server (edge) link
    Time s1uDelay{MilliSeconds(1)};        ///< Delay of the S1-U (gNB - SGW) links

    // O-RAN
    bool e2Network{true};  ///< Carry the E2 interface (UEs/gNBs <-> RIC) over the network
    bool ricNetwork{true}; ///< Query the RIC bitrate service over the network
    Time reportPeriod{MilliSeconds(100)};     ///< Period of the periodic E2 report triggers
    Time sendInterval{MilliSeconds(50)};      ///< Interval at which E2 terminators send reports
    Time holDelayTrigger{MilliSeconds(20)};   ///< HOL delay that triggers a UE stats report
    std::string dbFile{"oran-repository.db"}; ///< SQLite database file of the Near-RT RIC
    bool e2Trace{false}; ///< Print every E2 message sent/received over the network

    /**
     * Register the options with the command line.
     *
     * @param cmd The command line.
     */
    void AddCommandLineArgs(CommandLine& cmd)
    {
        cmd.AddValue("appRate", "VR application target data rate", appRate);
        cmd.AddValue("frameRate", "VR application frame rate [FPS]", frameRate);
        cmd.AddValue("vrAppName", "VR application name", vrAppName);
        cmd.AddValue("burstGeneratorType",
                     "VR adaptation algorithm {\"model\", \"google\", \"fuzzy\", \"gcc\", "
                     "\"nada\", \"bola\", \"mpc\", \"festive\", \"oran-util\", \"oran-util-udp\", "
                     "\"oran-util-udp-no-queue\", \"netxrate-udp\"}",
                     burstGeneratorType);
        cmd.AddValue("vrQosFlow",
                     "5QI of the dedicated QoS flow carrying the VR traffic (NrQosFlow name, "
                     "e.g. DGBR_VISUAL_CONTENT_89, NGBR_LOW_LAT_EMBB)",
                     vrQosFlow);
        cmd.AddValue("tcpType", "TCP variant of the TCP-based adaptation algorithms", tcpType);
        cmd.AddValue("pdbDiscard",
                     "Discard packets that wait at the gNB longer than the packet delay budget",
                     pdbDiscard);
        cmd.AddValue("ueTxPower", "UE TX power [dBm]", ueTxPower);
        cmd.AddValue("remoteHostDelay",
                     "One-way delay of the link between the PGW and the VR server",
                     remoteHostDelay);
        cmd.AddValue("s1uDelay", "One-way delay of the S1-U links", s1uDelay);
        cmd.AddValue("e2Network",
                     "Carry the E2 messages between the UEs/gNBs and the RIC over the network",
                     e2Network);
        cmd.AddValue("ricNetwork",
                     "Request the VR bitrate from the RIC xApp over the network",
                     ricNetwork);
        cmd.AddValue("reportPeriod", "Period of the periodic E2 reports", reportPeriod);
        cmd.AddValue("sendInterval", "Interval at which E2 terminators send reports", sendInterval);
        cmd.AddValue("holDelayTrigger",
                     "HOL delay that triggers an immediate UE stats report (0 disables)",
                     holDelayTrigger);
        cmd.AddValue("dbFile", "SQLite database file of the Near-RT RIC", dbFile);
        cmd.AddValue("e2Trace", "Print every E2 message sent/received over the network", e2Trace);
    }
};

/// What a burst generator type needs from the rest of the scenario.
struct BurstGeneratorSetup
{
    std::string socketFactory; ///< ns3::UdpSocketFactory or ns3::TcpSocketFactory
    bool usesOranLm{false};    ///< The algorithm queries OranLogicVrBitrate
    bool usesRicServer{false}; ///< The algorithm can reach OranLogicVrBitrate over the network
};

/**
 * Configure the BurstyApplicationServer adaptation algorithm for a burst generator type.
 *
 * Sets the adaptationAlgorithm default, the client congestion feedback for the UDP
 * delay-based algorithms, and the OranLogicVrBitrate of the O-RAN based algorithms.
 * Aborts on an unknown type.
 *
 * @param type The burst generator type.
 * @param lm The VR bitrate logic module used by the O-RAN based algorithms.
 * @return What the algorithm needs from the scenario.
 */
inline BurstGeneratorSetup
ConfigureBurstGenerator(const std::string& type, Ptr<OranLogicVrBitrate> lm)
{
    struct Entry
    {
        const char* type;        ///< Burst generator type
        bool tcp;                ///< Uses TCP
        const char* algorithm;   ///< BurstyApplicationServer::adaptationAlgorithm
        bool congestionFeedback; ///< Needs BurstyApplicationClient::SendCongestionFeedback
        const char* lmAttribute; ///< Attribute that receives the logic module, or nullptr
        bool ricServer;          ///< Has a RicServer attribute
    };

    static const Entry table[] = {
        {"model", false, "", false, nullptr, false},
        {"google", true, "GoogleAlgorithmServer", false, nullptr, false},
        {"fuzzy", true, "FuzzyAlgorithmServer", false, nullptr, false},
        {"gcc", false, "GccAlgorithmServer", true, nullptr, false},
        {"nada", false, "NadaAlgorithmServer", true, nullptr, false},
        {"bola", true, "BolaAlgo", false, nullptr, false},
        {"mpc", true, "MPCAlgo", false, nullptr, false},
        {"festive", true, "FestiveAlgorithm", false, nullptr, false},
        {"oran-util", true, "OranCellUtilizationAdaptationAlgorithm", false, nullptr, false},
        {"oran-util-udp",
         false,
         "OranCellUtilizationUdpAdaptationAlgorithm",
         false,
         "ns3::OranCellUtilizationUdpAdaptationAlgorithm::OranLogicVrBitrate",
         true},
        {"oran-util-udp-no-queue",
         false,
         "OranCellUtilizationUdpNoQueueAdaptationAlgorithm",
         false,
         "ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::OranLogicVrBitrate",
         true},
        {"netxrate-udp",
         false,
         "NetXRateUdpAdaptationAlgorithm",
         false,
         "ns3::NetXRateUdpAdaptationAlgorithm::OranLogicVrBitrate",
         false},
    };

    for (const Entry& e : table)
    {
        if (type != e.type)
        {
            continue;
        }
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue(e.algorithm));
        if (e.congestionFeedback)
        {
            Config::SetDefault("ns3::BurstyApplicationClient::SendCongestionFeedback",
                               BooleanValue(true));
        }
        if (e.lmAttribute)
        {
            Config::SetDefault(e.lmAttribute, PointerValue(lm));
        }
        BurstGeneratorSetup setup;
        setup.socketFactory = e.tcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory";
        setup.usesOranLm = e.lmAttribute != nullptr;
        setup.usesRicServer = e.ricServer;
        return setup;
    }
    NS_ABORT_MSG("Wrong burstGeneratorType: " << type);
    return {};
}

/**
 * @param t A time.
 * @return A ConstantRandomVariable string with the value of t in seconds.
 */
inline std::string
ConstantRv(Time t)
{
    return "ns3::ConstantRandomVariable[Constant=" + std::to_string(t.GetSeconds()) + "]";
}

/**
 * Set the defaults shared by all the scenarios: VR burst generator and server, RLC buffer,
 * TCP variant, E2 report period and the LCID queried by the O-RAN adaptation algorithms.
 *
 * @param opts The options.
 * @param streamDuration How long each server instance sends VR traffic.
 */
inline void
ConfigureCommonDefaults(const VrOranOptions& opts, Time streamDuration)
{
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(streamDuration));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize",
                       UintegerValue(VR_FRAGMENT_SIZE));
    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(opts.frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate",
                       DataRateValue(DataRate(opts.appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(opts.vrAppName));

    // Very large RLC buffers, so that the VR traffic is not dropped at the gNB
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::NrRlcUm::EnablePdcpDiscarding", BooleanValue(opts.pdbDiscard));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName(opts.tcpType)));

    Config::SetDefault("ns3::OranReportTriggerPeriodic::IntervalRv",
                       StringValue(ConstantRv(opts.reportPeriod)));

    // The O-RAN algorithms ask the RIC about the VR QoS flow
    Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::Lcid",
                       UintegerValue(VR_LCID));
    Config::SetDefault("ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::Lcid",
                       UintegerValue(VR_LCID));
    Config::SetDefault("ns3::NetXRateUdpAdaptationAlgorithm::Lcid", UintegerValue(VR_LCID));
}

/// The NR helpers of a scenario.
struct NrHelpers
{
    Ptr<NrPointToPointEpcHelper> epc;        ///< Core network
    Ptr<IdealBeamformingHelper> beamforming; ///< Beamforming
    Ptr<NrHelper> nr;                        ///< NR stack
};

/**
 * Create the NR helpers with the settings shared by all the scenarios: direct-path ideal
 * beamforming, OFDMA round robin scheduler and the S1-U delay.
 *
 * @param opts The options.
 * @return The helpers.
 */
inline NrHelpers
CreateNrHelpers(const VrOranOptions& opts)
{
    NrHelpers h;
    h.epc = CreateObject<NrPointToPointEpcHelper>();
    h.beamforming = CreateObject<IdealBeamformingHelper>();
    h.nr = CreateObject<NrHelper>();
    h.beamforming->SetAttribute("BeamformingMethod",
                                TypeIdValue(DirectPathBeamforming::GetTypeId()));
    h.beamforming->SetAttribute("BeamformingPeriodicity", TimeValue(MilliSeconds(100)));
    h.epc->SetAttribute("S1uLinkDelay", TimeValue(opts.s1uDelay));
    h.nr->SetBeamformingHelper(h.beamforming);
    h.nr->SetEpcHelper(h.epc);
    h.nr->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));
    return h;
}

/// The VR server node.
struct RemoteHost
{
    Ptr<Node> node;        ///< The remote host node
    Ipv4Address address;   ///< Its address
    Ptr<NetDevice> device; ///< Its device on the link to the PGW
};

/**
 * Create the VR server (an edge server behind the PGW: 100 Gb/s, MTU 2500 B, remoteHostDelay).
 *
 * @param epc The EPC helper.
 * @param opts The options.
 * @return The remote host.
 */
inline RemoteHost
CreateRemoteHost(Ptr<NrPointToPointEpcHelper> epc, const VrOranOptions& opts)
{
    auto [node, address] = epc->SetupRemoteHost("100Gb/s", 2500, opts.remoteHostDelay);
    // Device 0 is the loopback, device 1 the link to the PGW
    return {node, address, node->GetDevice(1)};
}

/**
 * Set the TX power of the UEs.
 *
 * @param ueDevs The UE devices.
 * @param txPower The TX power [dBm].
 */
inline void
SetUeTxPower(const NetDeviceContainer& ueDevs, double txPower)
{
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        NrHelper::GetUePhy(ueDevs.Get(i), 0)->SetAttribute("TxPower", DoubleValue(txPower));
    }
}

/**
 * Set the default route of every UE to the EPC gateway.
 *
 * @param ues The UE nodes, with the internet stack installed.
 * @param epc The EPC helper.
 */
inline void
SetUeDefaultRoutes(const NodeContainer& ues, Ptr<NrPointToPointEpcHelper> epc)
{
    Ipv4StaticRoutingHelper routing;
    for (uint32_t u = 0; u < ues.GetN(); ++u)
    {
        Ptr<Ipv4> ipv4 = ues.Get(u)->GetObject<Ipv4>();
        if (ipv4)
        {
            routing.GetStaticRouting(ipv4)->SetDefaultRoute(epc->GetUeDefaultGatewayAddress(), 1);
        }
    }
}

/**
 * @param name A 5QI name of NrQosFlow (e.g. "DGBR_VISUAL_CONTENT_89").
 * @return The corresponding 5QI.
 */
inline NrQosFlow::FiveQi
QosFlowFromName(const std::string& name)
{
    static const std::map<std::string, NrQosFlow::FiveQi> flows = {
        {"GBR_CONV_VOICE", NrQosFlow::GBR_CONV_VOICE},
        {"GBR_CONV_VIDEO", NrQosFlow::GBR_CONV_VIDEO},
        {"GBR_GAMING", NrQosFlow::GBR_GAMING},
        {"GBR_NON_CONV_VIDEO", NrQosFlow::GBR_NON_CONV_VIDEO},
        {"NGBR_VOICE_VIDEO_GAMING", NrQosFlow::NGBR_VOICE_VIDEO_GAMING},
        {"NGBR_VIDEO_TCP_DEFAULT", NrQosFlow::NGBR_VIDEO_TCP_DEFAULT},
        {"NGBR_LOW_LAT_EMBB", NrQosFlow::NGBR_LOW_LAT_EMBB},
        {"DGBR_INTER_SERV_87", NrQosFlow::DGBR_INTER_SERV_87},
        {"DGBR_INTER_SERV_88", NrQosFlow::DGBR_INTER_SERV_88},
        {"DGBR_VISUAL_CONTENT_89", NrQosFlow::DGBR_VISUAL_CONTENT_89},
        {"DGBR_VISUAL_CONTENT_90", NrQosFlow::DGBR_VISUAL_CONTENT_90},
    };
    auto it = flows.find(name);
    NS_ABORT_MSG_IF(it == flows.end(), "Unsupported vrQosFlow: " << name);
    return it->second;
}

/**
 * Activate the dedicated QoS flow of the VR traffic on every UE. The filter on the server
 * port matches the downlink (server is the remote source) and the uplink (server is the
 * remote destination), so the VR data, the client requests, the congestion feedback and the
 * TCP acknowledgements all use it.
 *
 * @param nrHelper The NR helper.
 * @param ueDevs The UE devices.
 * @param opts The options.
 */
inline void
ActivateVrQosFlow(Ptr<NrHelper> nrHelper,
                  const NetDeviceContainer& ueDevs,
                  const VrOranOptions& opts)
{
    if (opts.vrQosFlow == "default")
    {
        return; // the VR traffic stays on the default QoS flow
    }
    NrQosFlow vrFlow(QosFlowFromName(opts.vrQosFlow));
    Ptr<NrQosRule> vrRule = Create<NrQosRule>();
    NrQosRule::PacketFilter vrFilter;
    vrFilter.remotePortStart = VR_PORT;
    vrFilter.remotePortEnd = VR_PORT;
    vrRule->Add(vrFilter);
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->ActivateDedicatedQosFlow(ueDevs.Get(i), vrFlow, vrRule);
    }
}

/// The node that hosts the RIC services.
struct RicHost
{
    Ptr<Node> node;      ///< The RIC host node
    Ipv4Address address; ///< Its address on the link to the PGW
};

/**
 * Create the RIC host: a node behind the PGW (1 Gb/s, 1 ms link, network 2.0.0.0/8),
 * reachable from the remote host and from the gNBs.
 *
 * @param pgw The PGW node.
 * @param remoteHost The remote host, which gets a route to the RIC host.
 * @return The RIC host.
 */
inline RicHost
CreateRicHost(Ptr<Node> pgw, Ptr<Node> remoteHost)
{
    NodeContainer ricHostContainer;
    ricHostContainer.Create(1);
    Ptr<Node> ricHost = ricHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(ricHostContainer);

    PointToPointHelper ricP2p;
    ricP2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gb/s")));
    ricP2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    NetDeviceContainer ricDevices = ricP2p.Install(pgw, ricHost);
    Ipv4AddressHelper ricIpv4;
    ricIpv4.SetBase("2.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ricIfaces = ricIpv4.Assign(ricDevices);

    Ipv4StaticRoutingHelper routing;
    routing.GetStaticRouting(ricHost->GetObject<Ipv4>())
        ->SetDefaultRoute(ricIfaces.GetAddress(0), 1);
    routing.GetStaticRouting(remoteHost->GetObject<Ipv4>())
        ->AddNetworkRouteTo(Ipv4Address("2.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    return {ricHost, ricIfaces.GetAddress(1)};
}

/**
 * Install the VR bitrate service on the RIC host and point the O-RAN UDP adaptation
 * algorithms to it.
 *
 * @param ricHost The RIC host.
 * @param lm The VR bitrate logic module that answers the requests.
 * @param stopTime The stop time of the service.
 */
inline void
InstallRicBitrateServer(const RicHost& ricHost, Ptr<OranLogicVrBitrate> lm, Time stopTime)
{
    Ptr<OranVrBitrateServer> ricServer = CreateObject<OranVrBitrateServer>();
    ricServer->SetAttribute("OranLogicVrBitrate", PointerValue(lm));
    ricServer->SetAttribute("Port", UintegerValue(RIC_BITRATE_PORT));
    ricHost.node->AddApplication(ricServer);
    ricServer->SetStartTime(Seconds(0));
    ricServer->SetStopTime(stopTime);

    const AddressValue ricAddr(InetSocketAddress(ricHost.address, RIC_BITRATE_PORT));
    Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::RicServer", ricAddr);
    Config::SetDefault("ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::RicServer", ricAddr);
}

/**
 * Carry the E2 registrations, reports and commands between the UEs/gNBs and the RIC over the
 * simulated network (radio uplink for the UEs, S1-U/core for the gNBs).
 *
 * @param ric The Near-RT RIC.
 * @param ricHost The node that terminates the E2 interface.
 * @return The E2 network transport.
 */
inline Ptr<OranE2NetworkTransport>
EnableE2NetworkTransport(Ptr<OranNearRtRic> ric, Ptr<Node> ricHost)
{
    Ptr<OranE2NetworkTransport> e2Transport = CreateObject<OranE2NetworkTransport>();
    e2Transport->SetAttribute("RicNode", PointerValue(ricHost));
    ric->GetE2Terminator()->SetAttribute("NetworkTransport", PointerValue(e2Transport));
    return e2Transport;
}

/**
 * Create an OranHelper with an SQLite data repository (the old database is deleted), the
 * no-op conflict mitigation module, a 200 ms logic module query interval and no wait for
 * late logic modules. The caller sets the default logic module.
 *
 * @param opts The options.
 * @return The O-RAN helper.
 */
inline Ptr<OranHelper>
CreateOranHelper(const VrOranOptions& opts)
{
    std::remove(opts.dbFile.c_str());
    std::remove((opts.dbFile + "-wal").c_str());
    std::remove((opts.dbFile + "-shm").c_str());

    Ptr<OranHelper> oranHelper = CreateObject<OranHelper>();
    oranHelper->SetAttribute("Verbose", BooleanValue(false));
    oranHelper->SetAttribute("LmQueryInterval", TimeValue(MilliSeconds(200)));
    oranHelper->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(2)));
    oranHelper->SetAttribute("E2NodeInactivityIntervalRv", StringValue(ConstantRv(Seconds(2))));
    oranHelper->SetAttribute("LmQueryMaxWaitTime", TimeValue(Seconds(0)));
    oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue("DROP"));
    oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite",
                                  "DatabaseFile",
                                  StringValue(opts.dbFile));
    oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");
    return oranHelper;
}

/**
 * Create the E2 terminator of a UE with location, cell info and RSRP/RSRQ reporters, attach
 * it to the UE and schedule its activation at ORAN_START.
 *
 * @param ue The UE node.
 * @param ric The Near-RT RIC.
 * @param sendInterval The report send interval.
 * @return The UE terminator.
 */
inline Ptr<OranE2NodeTerminatorNrUe>
DeployUeTerminator(Ptr<Node> ue, Ptr<OranNearRtRic> ric, Time sendInterval)
{
    Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
    Ptr<OranReporterNrUeCellInfo> cellInfoReporter = CreateObject<OranReporterNrUeCellInfo>();
    Ptr<OranReporterNrUeRsrpRsrq> rsrpReporter = CreateObject<OranReporterNrUeRsrpRsrq>();
    Ptr<OranE2NodeTerminatorNrUe> ueTerminator = CreateObject<OranE2NodeTerminatorNrUe>();

    locationReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
    cellInfoReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
    rsrpReporter->SetAttribute("Terminator", PointerValue(ueTerminator));

    for (uint32_t d = 0; d < ue->GetNDevices(); ++d)
    {
        Ptr<NrUeNetDevice> nrUeDevice = ue->GetDevice(d)->GetObject<NrUeNetDevice>();
        if (nrUeDevice)
        {
            nrUeDevice->GetPhy(0)->TraceConnectWithoutContext(
                "ReportUeMeasurements",
                MakeCallback(&OranReporterNrUeRsrpRsrq::ReportRsrpRsrq, rsrpReporter));
        }
    }

    ueTerminator->SetAttribute("NearRtRic", PointerValue(ric));
    ueTerminator->SetAttribute("RegistrationIntervalRv",
                               StringValue(ConstantRv(E2_REGISTRATION_INTERVAL)));
    ueTerminator->SetAttribute("SendIntervalRv", StringValue(ConstantRv(sendInterval)));
    ueTerminator->AddReporter(locationReporter);
    ueTerminator->AddReporter(cellInfoReporter);
    ueTerminator->AddReporter(rsrpReporter);
    ueTerminator->Attach(ue);

    Simulator::Schedule(ORAN_START, &OranE2NodeTerminatorNrUe::Activate, ueTerminator);
    return ueTerminator;
}

/**
 * Feed the buffer status reports and the scheduler statistics of each gNB MAC to the
 * OranReporterNrUeStats reporters of that gNB's terminator.
 *
 * @param gnbDevs The gNB devices.
 * @param gnbTerminators The terminators deployed on the gNB nodes.
 */
inline void
ConnectUeStatsReporters(const NetDeviceContainer& gnbDevs,
                        const OranE2NodeTerminatorContainer& gnbTerminators)
{
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        Ptr<NrGnbNetDevice> gnbDev = gnbDevs.Get(i)->GetObject<NrGnbNetDevice>();
        if (!gnbDev)
        {
            continue;
        }
        for (uint32_t t = 0; t < gnbTerminators.GetN(); ++t)
        {
            Ptr<OranE2NodeTerminator> term = gnbTerminators.Get(t);
            if (term->GetNode() != gnbDev->GetNode())
            {
                continue;
            }
            ObjectVectorValue reporters;
            term->GetAttribute("Reporters", reporters);
            for (std::size_t r = 0; r < reporters.GetN(); ++r)
            {
                Ptr<OranReporterNrUeStats> stats =
                    DynamicCast<OranReporterNrUeStats>(reporters.Get(r));
                if (!stats)
                {
                    continue;
                }
                for (uint32_t m = 0; m < gnbDev->GetCcMapSize(); ++m)
                {
                    gnbDev->GetMac(m)->TraceConnectWithoutContext(
                        "BufferStatusReportTrace",
                        MakeCallback(&OranReporterNrUeStats::OnBufferStatusReport, stats));
                    Ptr<NrMacScheduler> sched = NrHelper::GetScheduler(gnbDevs.Get(i), m);
                    if (sched)
                    {
                        sched->TraceConnectWithoutContext(
                            "SchedStats",
                            MakeCallback(&OranReporterNrUeStats::OnSchedStats, stats));
                    }
                }
            }
        }
    }
}

/// Per-node statistics of the E2 messages carried over the network.
class E2TransportStats
{
  public:
    bool printEachMessage{false}; ///< Print every E2 message as it is sent/received

    /**
     * Collect the statistics of an E2 network transport.
     *
     * @param transport The E2 network transport.
     * @param ricNodeId The node id of the RIC host, marked in the output.
     */
    void Connect(Ptr<OranE2NetworkTransport> transport, uint32_t ricNodeId)
    {
        m_connected = true;
        m_ricNodeId = ricNodeId;
        transport->TraceConnectWithoutContext("E2Tx", MakeCallback(&E2TransportStats::Tx, this));
        transport->TraceConnectWithoutContext("E2Rx", MakeCallback(&E2TransportStats::Rx, this));
    }

    /**
     * Print the statistics, if the E2 interface was carried over the network.
     *
     * @param os The output stream.
     */
    void Print(std::ostream& os) const
    {
        if (!m_connected)
        {
            return;
        }
        os << "\nE2 transport statistics (node, role, tx msgs, rx msgs, mean/max delay ms):\n";
        for (const auto& [id, st] : m_stats)
        {
            os << "  node " << id << (id == m_ricNodeId ? " (RIC)" : "") << ": tx " << st.txMsgs
               << " (" << st.txBytes << " B), rx " << st.rxMsgs << " (" << st.rxBytes << " B)";
            if (st.rxMsgs)
            {
                os << ", delay " << st.delaySumMs / st.rxMsgs << " / " << st.delayMaxMs;
            }
            os << "\n";
        }
    }

  private:
    /// Counters of one node.
    struct NodeStats
    {
        uint32_t txMsgs{0};   ///< Messages sent
        uint64_t txBytes{0};  ///< Bytes sent
        uint32_t rxMsgs{0};   ///< Messages received
        uint64_t rxBytes{0};  ///< Bytes received
        double delaySumMs{0}; ///< Sum of the delays of the received messages [ms]
        double delayMaxMs{0}; ///< Max delay of the received messages [ms]
    };

    /**
     * E2Tx trace sink.
     *
     * @param id The message id.
     * @param node The sending node.
     * @param dest The destination address.
     * @param size The message size [bytes].
     */
    void Tx(uint64_t id, Ptr<const Node> node, Ipv4Address dest, uint32_t size)
    {
        auto& st = m_stats[node->GetId()];
        st.txMsgs++;
        st.txBytes += size;
        if (printEachMessage)
        {
            std::cout << Simulator::Now().As(Time::S) << " E2 TX msg " << id << " node "
                      << node->GetId() << " -> " << dest << " " << size << " B" << std::endl;
        }
    }

    /**
     * E2Rx trace sink.
     *
     * @param id The message id.
     * @param node The receiving node.
     * @param delay The message delay.
     * @param size The message size [bytes].
     */
    void Rx(uint64_t id, Ptr<const Node> node, Time delay, uint32_t size)
    {
        auto& st = m_stats[node->GetId()];
        st.rxMsgs++;
        st.rxBytes += size;
        st.delaySumMs += delay.GetSeconds() * 1e3;
        st.delayMaxMs = std::max(st.delayMaxMs, delay.GetSeconds() * 1e3);
        if (printEachMessage)
        {
            std::cout << Simulator::Now().As(Time::S) << " E2 RX msg " << id << " node "
                      << node->GetId() << " " << size << " B, delay " << delay.GetMilliSeconds()
                      << " ms" << std::endl;
        }
    }

    std::map<uint32_t, NodeStats> m_stats; ///< Statistics by node id
    uint32_t m_ricNodeId{0};               ///< Node id of the RIC host
    bool m_connected{false};               ///< Connect() was called
};

/// The deployed O-RAN components.
struct OranDeployment
{
    Ptr<OranNearRtRic> ric;                       ///< The Near-RT RIC
    OranE2NodeTerminatorContainer gnbTerminators; ///< The gNB E2 terminators
    E2TransportStats e2Stats;                     ///< E2 transport statistics
};

/**
 * Deploy the O-RAN components, identically in every scenario and for every algorithm:
 *  - the Near-RT RIC (the caller has set its logic module on oranHelper), whose data
 *    repository is shared with the VR bitrate logic module;
 *  - the RIC host behind the PGW, if the E2 interface or the bitrate queries use the network,
 *    with the bitrate service if the algorithm queries it;
 *  - one E2 terminator per UE (location, cell info, RSRP/RSRQ reporters) and per gNB
 *    (location and OranReporterNrUeStats reporters, fed by the MAC and scheduler traces);
 *  - their activation: RIC and UE terminators at ORAN_START, gNB terminators at
 *    ORAN_GNB_START.
 *
 * @param oranHelper The O-RAN helper (from CreateOranHelper).
 * @param opts The options.
 * @param generator The burst generator setup.
 * @param lm The VR bitrate logic module.
 * @param pgw The PGW node.
 * @param remoteHost The VR server node.
 * @param gnbNodes The gNB nodes.
 * @param gnbDevs The gNB devices.
 * @param ueNodes The UE nodes.
 * @param stopTime The stop time of the RIC bitrate service.
 * @param deployment The deployed components (output; not copyable because of the trace sinks).
 */
inline void
DeployOran(Ptr<OranHelper> oranHelper,
           const VrOranOptions& opts,
           const BurstGeneratorSetup& generator,
           Ptr<OranLogicVrBitrate> lm,
           Ptr<Node> pgw,
           Ptr<Node> remoteHost,
           const NodeContainer& gnbNodes,
           const NetDeviceContainer& gnbDevs,
           const NodeContainer& ueNodes,
           Time stopTime,
           OranDeployment& deployment)
{
    deployment.ric = oranHelper->CreateNearRtRic();
    lm->SetDataRepository(deployment.ric->Data());

    const bool ricBitrateOverNetwork = opts.ricNetwork && generator.usesRicServer;
    if (ricBitrateOverNetwork || opts.e2Network)
    {
        RicHost ricHost = CreateRicHost(pgw, remoteHost);
        if (ricBitrateOverNetwork)
        {
            InstallRicBitrateServer(ricHost, lm, stopTime);
        }
        if (opts.e2Network)
        {
            deployment.e2Stats.printEachMessage = opts.e2Trace;
            deployment.e2Stats.Connect(EnableE2NetworkTransport(deployment.ric, ricHost.node),
                                       ricHost.node->GetId());
        }
    }

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        DeployUeTerminator(ueNodes.Get(i), deployment.ric, opts.sendInterval);
    }

    oranHelper->SetE2NodeTerminator("ns3::OranE2NodeTerminatorNrGnb",
                                    "RegistrationIntervalRv",
                                    StringValue(ConstantRv(E2_REGISTRATION_INTERVAL)),
                                    "SendIntervalRv",
                                    StringValue(ConstantRv(opts.sendInterval)));
    oranHelper->AddReporter("ns3::OranReporterLocation",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    oranHelper->AddReporter("ns3::OranReporterNrUeStats",
                            "HolDelayTrigger",
                            TimeValue(opts.holDelayTrigger),
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    deployment.gnbTerminators = oranHelper->DeployTerminators(deployment.ric, gnbNodes);
    ConnectUeStatsReporters(gnbDevs, deployment.gnbTerminators);

    Simulator::Schedule(ORAN_START,
                        &OranHelper::ActivateAndStartNearRtRic,
                        oranHelper,
                        deployment.ric);
    Simulator::Schedule(ORAN_GNB_START,
                        &OranHelper::ActivateE2NodeTerminators,
                        oranHelper,
                        deployment.gnbTerminators);
}

/**
 * Install the VR server (BurstyApplicationServer) on the remote host.
 *
 * @param socketFactory The socket factory type id.
 * @param remoteHost The remote host.
 * @param stopTime The stop time of the server.
 * @return The server application.
 */
inline ApplicationContainer
InstallVrServer(const std::string& socketFactory, Ptr<Node> remoteHost, Time stopTime)
{
    BurstyApplicationServerHelper server(socketFactory,
                                         InetSocketAddress(Ipv4Address::GetAny(), VR_PORT));
    ApplicationContainer serverApp = server.Install(remoteHost);
    serverApp.Start(Seconds(0));
    serverApp.Stop(stopTime);
    return serverApp;
}

/**
 * Install a VR client (BurstyApplicationClient) on each UE, bound to the UE address.
 * The caller sets the start times.
 *
 * @param socketFactory The socket factory type id.
 * @param server The address of the VR server.
 * @param ues The UE nodes.
 * @param ueIfaces The UE IPv4 interfaces, in the same order as ues.
 * @param stopTime The stop time of the clients.
 * @return The client applications, in the same order as ues.
 */
inline ApplicationContainer
InstallVrClients(const std::string& socketFactory,
                 Ipv4Address server,
                 const NodeContainer& ues,
                 const Ipv4InterfaceContainer& ueIfaces,
                 Time stopTime)
{
    BurstyApplicationClientHelper client(socketFactory, InetSocketAddress(server, VR_PORT));
    ApplicationContainer clientApps = client.Install(ues);
    for (uint32_t u = 0; u < clientApps.GetN(); ++u)
    {
        clientApps.Get(u)->SetAttribute("Local",
                                        AddressValue(InetSocketAddress(ueIfaces.GetAddress(u), 0)));
    }
    clientApps.Stop(stopTime);
    return clientApps;
}

} // namespace vrnr
} // namespace ns3

#endif // VR_NR_ORAN_COMMON_H
