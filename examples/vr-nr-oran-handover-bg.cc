// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-nr-oran-handover-bg.cc
 * @brief vr-nr-oran-handover with background VR traffic: one stationary UE next to each gNB.
 *
 * Same scenario as vr-nr-oran-handover.cc (a row of gNBs, one UE moving along it, handovers
 * triggered by OranLmNr2NrRsrpHandover, VR traffic from a remote host), but in addition one
 * stationary UE is placed next to every gNB (--backgroundUes) and receives the same VR traffic.
 * This shows the adaptation algorithm (e.g. --burstGeneratorType=oran-util-udp) sharing the
 * capacity of each cell between several UEs, and the moving UE changing the load of the cells
 * it is handed over to.
 *
 * Output (<prefix> is set with --outPrefix):
 *  - Each handover is printed on the terminal as it happens, plus a per-interval summary line.
 *  - <prefix>-timeseries.csv: per-UE, per-interval position, serving cell, received VR throughput,
 *    mean delay and packet loss. UE 0 is the moving one ("moving"), the others are "static".
 *  - <prefix>-gnb-positions.csv: gNB locations.
 *  - <prefix>-gnb-load.csv: per-gNB downlink throughput and RSRP of the moving UE.
 *  - <prefix>-gnb-dl.csv: one line per downlink packet reaching a gNB from the core network.
 * plot-vr-handover-bg.py plots the results of a run with and without --handover.
 */

// NOLINTBEGIN
// clang-format off

#include "ns3/applications-module.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-client.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/oran-module.h"
#include "ns3/oran-reporter-nr-ue-stats.h"
#include "ns3/point-to-point-module.h"

#include <cstdio>
#include <set>
#include <map>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VrNrOranHandover");

namespace
{

/// Statistics of one UE accumulated during one reporting interval.
struct UeInterval
{
    uint64_t rxBytes{0};    ///< VR bytes received by the UE
    double delaySum{0};     ///< Sum of the VR fragment delays [s]
    uint64_t delayCount{0}; ///< Number of VR fragments in delaySum
};

/// One row of the per-UE time series, written at the end of the simulation (loss is known then).
struct Row
{
    double t;       ///< Time [s]
    uint32_t ue;    ///< UE index (0 is the moving UE)
    Vector uePos;   ///< UE position
    uint16_t cell;  ///< Serving cell
    double rxMbps;  ///< VR throughput received by the UE [Mbps]
    double delayMs; ///< Mean VR fragment delay [ms]
};

/// One row of the per-gNB load time series.
struct GnbRow
{
    double t;                    ///< Time [s]
    std::vector<double> gnbMbps; ///< Per-gNB downlink throughput [Mbps]
    std::vector<double> rsrp;    ///< RSRP of the moving UE towards each gNB [dBm]
};

std::vector<UeInterval> g_ueStats;         ///< Current interval statistics, per UE
std::vector<uint64_t> g_gnbDlBytes;        ///< Downlink bytes reaching each gNB in the interval
std::map<uint32_t, uint32_t> g_nodeToGnb;  ///< Node id -> gNB index
std::map<uint32_t, uint32_t> g_ipToUe;     ///< UE IPv4 address -> UE index
std::vector<uint16_t> g_cellIds;           ///< Cell id of each gNB
std::vector<double> g_rsrp;                ///< Last RSRP [dBm] of the moving UE per gNB index
Ptr<OutputStreamWrapper> g_gnbDlTrace;     ///< Per-packet gNB downlink trace
Ptr<OutputStreamWrapper> g_summaryTrace;   ///< Per-UE, per-interval summary trace
Ptr<OutputStreamWrapper> g_gnbLoadTrace;   ///< Per-gNB load trace
std::vector<Ptr<NrUeNetDevice>> g_ueDevs;  ///< The UE devices
std::vector<Ptr<MobilityModel>> g_ueMob;   ///< The UE mobility models
std::vector<double> g_ueTotalRxBytes;      ///< Total VR bytes received by each UE
Time g_interval = Seconds(1);              ///< Reporting interval
uint32_t g_handoverCount = 0;              ///< Handovers completed so far
Time g_stopReporting;                      ///< No reports are produced after this time
std::string g_prefix = "vr-handover-bg";   ///< Prefix of the output files
std::vector<Row> g_rows;                   ///< Per-UE time series
std::vector<GnbRow> g_gnbRows;             ///< Per-gNB time series
std::map<uint64_t, uint32_t> g_sentFragments; ///< Fragment key -> interval in which it was sent
std::set<uint64_t> g_rxFragments;             ///< Keys of the fragments received by the UEs

/// @return a key identifying a VR fragment of a given UE (burst sequence and fragment sequence).
uint64_t
FragmentKey(uint32_t ue, const SeqTsSizeFragHeader& h)
{
    return (static_cast<uint64_t>(ue) << 56) | ((static_cast<uint64_t>(h.GetSeq()) & 0xFFFFFF) << 32) |
           h.GetFragSeq();
}

/// Called when the VR server sends an IP packet towards the core network.
void
ServerTx(Ptr<const Packet> packet)
{
    Ptr<Packet> p = packet->Copy();
    PppHeader ppp;
    Ipv4Header ip;
    UdpHeader udp;
    SeqTsSizeFragHeader frag;
    p->RemoveHeader(ppp);
    if (p->PeekHeader(ip) == 0 || ip.GetProtocol() != 17)
    {
        return;
    }
    auto it = g_ipToUe.find(ip.GetDestination().Get());
    if (it == g_ipToUe.end())
    {
        return;
    }
    p->RemoveHeader(ip);
    p->RemoveHeader(udp);
    p->PeekHeader(frag);
    g_sentFragments.emplace(FragmentKey(it->second, frag),
                            static_cast<uint32_t>(Simulator::Now().GetSeconds() /
                                                  g_interval.GetSeconds()));
}

/// @return the gNB index of the given cell id, or -1.
int
GnbIndexOfCell(uint16_t cellId)
{
    for (size_t i = 0; i < g_cellIds.size(); ++i)
    {
        if (g_cellIds[i] == cellId)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// Extract the node id from a Config path context ("/NodeList/<id>/...").
uint32_t
NodeIdFromContext(const std::string& context)
{
    size_t pos = context.find("/NodeList/") + 10;
    return std::stoul(context.substr(pos, context.find('/', pos) - pos));
}

void
GnbRxFromS1u(std::string context, Ptr<Packet> packet)
{
    auto it = g_nodeToGnb.find(NodeIdFromContext(context));
    if (it == g_nodeToGnb.end())
    {
        return;
    }
    g_gnbDlBytes[it->second] += packet->GetSize();
    *g_gnbDlTrace->GetStream() << Simulator::Now().GetSeconds() << ","
                               << g_cellIds[it->second] << "," << packet->GetSize() << "\n";
}

void
FragmentRx(uint32_t ue,
           Ptr<const Packet> fragment,
           const Address& from,
           const Address& to,
           const SeqTsSizeFragHeader& header)
{
    g_rxFragments.insert(FragmentKey(ue, header));
    g_ueStats[ue].rxBytes += fragment->GetSize();
    g_ueTotalRxBytes[ue] += fragment->GetSize();
    g_ueStats[ue].delaySum += (Simulator::Now() - header.GetTs()).GetSeconds();
    g_ueStats[ue].delayCount++;
}

/// RSRP measurements of the moving UE.
void
UeMeasurements(uint16_t rnti,
               uint16_t cellId,
               double rsrp,
               double rsrq,
               bool isServingCell,
               uint8_t componentCarrierId)
{
    int idx = GnbIndexOfCell(cellId);
    if (idx >= 0)
    {
        g_rsrp[idx] = rsrp;
    }
}

void
HandoverStart(uint64_t imsi, uint16_t cellId, uint16_t rnti, uint16_t targetCellId)
{
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] HANDOVER START:    UE IMSI " << imsi << " cell " << cellId << " -> cell "
              << targetCellId << std::endl;
}

void
HandoverEndOk(uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    g_handoverCount++;
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] HANDOVER COMPLETE: UE IMSI " << imsi << " is now in cell " << cellId
              << " (RNTI " << rnti << ") - handover #" << g_handoverCount << std::endl;
}

void
RadioLinkFailure(uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] RADIO LINK FAILURE: UE IMSI " << imsi << " in cell " << cellId << std::endl;
}

void
Report()
{
    const double t = Simulator::Now().GetSeconds();
    const double dt = g_interval.GetSeconds();

    std::ostringstream line;
    line << std::fixed << std::setprecision(1) << t << ",x=" << g_ueMob[0]->GetPosition().x;
    for (uint32_t u = 0; u < g_ueDevs.size(); ++u)
    {
        Row row;
        row.t = t;
        row.ue = u;
        row.uePos = g_ueMob[u]->GetPosition();
        row.cell = g_ueDevs[u]->GetRrc()->GetCellId();
        row.rxMbps = g_ueStats[u].rxBytes * 8 / dt / 1e6;
        row.delayMs = g_ueStats[u].delayCount
                          ? g_ueStats[u].delaySum / g_ueStats[u].delayCount * 1000
                          : 0.0;
        g_rows.push_back(row);
        line << " | UE" << u << " cell" << row.cell << " " << row.rxMbps << "Mbps "
             << row.delayMs << "ms";
        g_ueStats[u] = UeInterval();
    }
    GnbRow gr;
    gr.t = t;
    for (uint64_t bytes : g_gnbDlBytes)
    {
        gr.gnbMbps.push_back(bytes * 8 / dt / 1e6);
    }
    gr.rsrp = g_rsrp;
    g_gnbRows.push_back(gr);
    std::cout << line.str() << std::endl;

    g_gnbDlBytes.assign(g_cellIds.size(), 0);
    if (Simulator::Now() + g_interval <= g_stopReporting)
    {
        Simulator::Schedule(g_interval, &Report);
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    // Scenario parameters
    bool handover = true;          // enable the O-RAN handover xApp
    uint16_t numGnbs = 3;          // gNBs placed in a row
    double gnbSpacing = 200;       // distance between neighbouring gNBs [m]
    double speed = 20;             // UE speed [m/s]
    double gnbHeight = 10;         // gNB antenna height [m]
    double ueOffsetY = 20;         // lateral distance of the UE path from the gNB row [m]
    bool backgroundUes = true;     // one stationary UE next to each gNB, with the same VR traffic
    double bgOffsetX = 10;         // x offset of the stationary UEs from their gNB [m]
    double bgOffsetY = 10;         // y offset of the stationary UEs from their gNB [m]
    Time simTime = Seconds(22);

    // Radio parameters
    double centralFrequency = 3.5e9;
    double bandwidth = 40e6;
    uint16_t numerology = 1;
    double txPower = 20; // gNB total TX power [dBm]

    // VR traffic
    std::string appRate = "20Mbps";
    double frameRate = 60;
    std::string vrAppName = "VirusPopper";
    std::string burstGeneratorType = "model"; // {"model", "oran-util-udp", "oran-util-udp-no-queue"}

    // O-RAN
    Time lmQueryInterval = Seconds(1);
    std::string dbFileName = "oran-vr-handover.db";

    CommandLine cmd(__FILE__);
    cmd.AddValue("handover", "Enable the O-RAN handover logic module", handover);
    cmd.AddValue("numGnbs", "Number of gNBs in the row", numGnbs);
    cmd.AddValue("gnbSpacing", "Distance between neighbouring gNBs [m]", gnbSpacing);
    cmd.AddValue("speed", "UE speed [m/s]", speed);
    cmd.AddValue("ueOffsetY", "Lateral distance of the UE path from the gNB row [m]", ueOffsetY);
    cmd.AddValue("outPrefix", "Prefix of the output files", g_prefix);
    cmd.AddValue("backgroundUes",
                 "Add one stationary UE with VR traffic next to each gNB",
                 backgroundUes);
    cmd.AddValue("bgOffsetX", "x offset of the stationary UEs from their gNB [m]", bgOffsetX);
    cmd.AddValue("bgOffsetY", "y offset of the stationary UEs from their gNB [m]", bgOffsetY);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("frequency", "Central frequency [Hz]", centralFrequency);
    cmd.AddValue("bandwidth", "Bandwidth [Hz]", bandwidth);
    cmd.AddValue("numerology", "NR numerology", numerology);
    cmd.AddValue("txPower", "gNB total TX power [dBm]", txPower);
    cmd.AddValue("appRate", "VR application target data rate", appRate);
    cmd.AddValue("frameRate", "VR application frame rate [FPS]", frameRate);
    cmd.AddValue("burstGeneratorType",
                 "VR burst generator {\"model\", \"oran-util-udp\", \"oran-util-udp-no-queue\"}",
                 burstGeneratorType);
    cmd.AddValue("vrAppName", "VR application name", vrAppName);
    cmd.AddValue("lmQueryInterval", "Interval at which the RIC queries the LM", lmQueryInterval);
    cmd.AddValue("reportInterval", "Interval of the summary output", g_interval);
    cmd.Parse(argc, argv);
    g_stopReporting = simTime;

    NS_ABORT_MSG_IF(numGnbs < 2, "At least two gNBs are needed");

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(simTime));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(1200));
    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate", DataRateValue(DataRate(appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(vrAppName));

    LogComponentEnable("OranNearRtRic", (LogLevel)(LOG_PREFIX_TIME | LOG_WARN));

    // Core network and remote host (the VR server)
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    beamformingHelper->SetAttribute("BeamformingMethod",
                                    TypeIdValue(DirectPathBeamforming::GetTypeId()));
    beamformingHelper->SetAttribute("BeamformingPeriodicity", TimeValue(MilliSeconds(100)));
    nrHelper->SetBeamformingHelper(beamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Nodes and mobility: gNBs in a row, the UE moves along it
    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(numGnbs);
    const uint32_t numUes = 1 + (backgroundUes ? numGnbs : 0); // UE 0 moves, UE i>0 sits at gNB i-1
    ueNodes.Create(numUes);

    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < numGnbs; ++i)
    {
        gnbPos->Add(Vector(gnbSpacing * i, 0, gnbHeight));
    }
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.SetPositionAllocator(gnbPos);
    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobilityHelper;
    ueMobilityHelper.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobilityHelper.Install(ueNodes);
    Ptr<ConstantVelocityMobilityModel> ueMobility =
        ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    ueMobility->SetPosition(Vector(0, ueOffsetY, 1.5));
    ueMobility->SetVelocity(Vector(speed, 0, 0));
    for (uint32_t u = 1; u < numUes; ++u)
    {
        Ptr<ConstantVelocityMobilityModel> bgMob =
            ueNodes.Get(u)->GetObject<ConstantVelocityMobilityModel>();
        bgMob->SetPosition(Vector(gnbSpacing * (u - 1) + bgOffsetX, bgOffsetY, 1.5));
        bgMob->SetVelocity(Vector(0, 0, 0));
    }

    // NR spectrum
    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        NrHelper::GetGnbPhy(gnbDevs.Get(i), 0)->SetAttribute("Numerology", UintegerValue(numerology));
        NrHelper::GetGnbPhy(gnbDevs.Get(i), 0)->SetAttribute("TxPower", DoubleValue(txPower));
    }

    // IP configuration of the UE
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    for (uint32_t u = 0; u < numUes; ++u)
    {
        ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(u)->GetObject<Ipv4>())
            ->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // X2 between all gNBs (needed for X2 handover) and initial attachment to the first gNB
    nrHelper->AddX2Interface(gnbNodes);
    nrHelper->AttachToGnb(ueDevs.Get(0), gnbDevs.Get(0));
    for (uint32_t u = 1; u < numUes; ++u)
    {
        nrHelper->AttachToGnb(ueDevs.Get(u), gnbDevs.Get(u - 1));
    }

    for (uint32_t u = 0; u < numUes; ++u)
    {
        g_ueDevs.push_back(ueDevs.Get(u)->GetObject<NrUeNetDevice>());
        g_ueMob.push_back(ueNodes.Get(u)->GetObject<MobilityModel>());
        g_ipToUe[ueIpIface.GetAddress(u).Get()] = u;
    }
    g_ueStats.assign(numUes, UeInterval());
    g_ueTotalRxBytes.assign(numUes, 0);
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        g_nodeToGnb[gnbNodes.Get(i)->GetId()] = i;
        g_cellIds.push_back(gnbDevs.Get(i)->GetObject<NrGnbNetDevice>()->GetCellId());
    }
    g_gnbDlBytes.assign(numGnbs, 0);
    g_rsrp.assign(numGnbs, std::nan(""));

    // Downlink VR traffic: server on the remote host, client on the UE
    const uint16_t port = 1234;
    const std::string protocol = "ns3::UdpSocketFactory";
    const bool oranUtil =
        (burstGeneratorType == "oran-util-udp" || burstGeneratorType == "oran-util-udp-no-queue");
    NS_ABORT_MSG_IF(!oranUtil && burstGeneratorType != "model",
                    "Wrong burstGeneratorType: " << burstGeneratorType);
    Ptr<OranLogicVrBitrate> vrBitrateLm = CreateObject<OranLogicVrBitrate>();
    if (burstGeneratorType == "model")
    {
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm", StringValue(""));
    }
    else if (burstGeneratorType == "oran-util-udp")
    {
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpAdaptationAlgorithm"));
        Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::OranLogicVrBitrate",
                           PointerValue(vrBitrateLm));
    }
    else
    {
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpNoQueueAdaptationAlgorithm"));
        Config::SetDefault(
            "ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::OranLogicVrBitrate",
            PointerValue(vrBitrateLm));
    }
    BurstyApplicationServerHelper serverHelper(protocol,
                                               InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = serverHelper.Install(remoteHost);
    serverApp.Start(Seconds(0));
    serverApp.Stop(simTime + Seconds(1));

    BurstyApplicationClientHelper clientHelper(protocol, InetSocketAddress(remoteHostAddr, port));
    ApplicationContainer clientApps = clientHelper.Install(ueNodes);
    for (uint32_t u = 0; u < numUes; ++u)
    {
        Ptr<BurstyApplicationClient> client =
            DynamicCast<BurstyApplicationClient>(clientApps.Get(u));
        client->SetAttribute("Local", AddressValue(InetSocketAddress(ueIpIface.GetAddress(u), 0)));
        client->SetStartTime(Seconds(1));
        client->SetStopTime(simTime + Seconds(1));
        client->TraceConnectWithoutContext("FragmentRx", MakeBoundCallback(&FragmentRx, u));
    }

    // O-RAN: Near-RT RIC with the NR RSRP handover logic module (or a no-op one)
    std::remove(dbFileName.c_str());
    std::remove((dbFileName + "-wal").c_str());
    std::remove((dbFileName + "-shm").c_str());

    Ptr<OranHelper> oranHelper = CreateObject<OranHelper>();
    oranHelper->SetAttribute("Verbose", BooleanValue(false));
    oranHelper->SetAttribute("LmQueryInterval", TimeValue(lmQueryInterval));
    oranHelper->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(2)));
    oranHelper->SetAttribute("E2NodeInactivityIntervalRv",
                             StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    oranHelper->SetAttribute("LmQueryMaxWaitTime", TimeValue(Seconds(0)));
    oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue("DROP"));
    oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite",
                                  "DatabaseFile",
                                  StringValue(dbFileName));
    if (handover)
    {
        oranHelper->SetDefaultLogicModule("ns3::OranLmNr2NrRsrpHandover",
                                          "ProcessingDelayRv",
                                          StringValue("ns3::ConstantRandomVariable[Constant=0.005]"));
    }
    else
    {
        oranHelper->SetDefaultLogicModule("ns3::OranLmNoop");
    }
    oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");
    Ptr<OranNearRtRic> nearRtRic = oranHelper->CreateNearRtRic();

    for (uint32_t u = 0; u < numUes; ++u)
    {
        Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
        Ptr<OranReporterNrUeCellInfo> cellInfoReporter = CreateObject<OranReporterNrUeCellInfo>();
        Ptr<OranReporterNrUeRsrpRsrq> rsrpReporter = CreateObject<OranReporterNrUeRsrpRsrq>();
        Ptr<OranE2NodeTerminatorNrUe> ueTerminator = CreateObject<OranE2NodeTerminatorNrUe>();
        locationReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
        cellInfoReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
        rsrpReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
        g_ueDevs[u]->GetPhy(0)->TraceConnectWithoutContext(
            "ReportUeMeasurements",
            MakeCallback(&OranReporterNrUeRsrpRsrq::ReportRsrpRsrq, rsrpReporter));
        if (u == 0)
        {
            g_ueDevs[u]->GetPhy(0)->TraceConnectWithoutContext("ReportUeMeasurements",
                                                               MakeCallback(&UeMeasurements));
        }
        ueTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
        ueTerminator->SetAttribute("RegistrationIntervalRv",
                                   StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
        ueTerminator->SetAttribute("SendIntervalRv",
                                   StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
        ueTerminator->AddReporter(locationReporter);
        ueTerminator->AddReporter(cellInfoReporter);
        ueTerminator->AddReporter(rsrpReporter);
        ueTerminator->Attach(ueNodes.Get(u));
        Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorNrUe::Activate, ueTerminator);
    }

    if (oranUtil)
    {
        oranHelper->AddReporter("ns3::OranReporterNrUeStats",
                                "Trigger",
                                StringValue("ns3::OranReportTriggerPeriodic"));
    }
    oranHelper->SetE2NodeTerminator("ns3::OranE2NodeTerminatorNrGnb",
                                    "RegistrationIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                                    "SendIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    oranHelper->AddReporter("ns3::OranReporterLocation",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    OranE2NodeTerminatorContainer gnbTerminators =
        oranHelper->DeployTerminators(nearRtRic, gnbNodes);

    if (oranUtil)
    {
        vrBitrateLm->SetDataRepository(nearRtRic->Data());
        // Feed the gNB MAC buffer status and scheduler statistics to the stats reporters
        for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
        {
            Ptr<NrGnbNetDevice> gnbDev = gnbDevs.Get(i)->GetObject<NrGnbNetDevice>();
            for (uint32_t t = 0; t < gnbTerminators.GetN(); ++t)
            {
                Ptr<OranE2NodeTerminator> term = gnbTerminators.Get(t);
                if (term->GetNode() != gnbNodes.Get(i))
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
                        Ptr<NrMacScheduler> sched = nrHelper->GetScheduler(gnbDevs.Get(i), m);
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

    Simulator::Schedule(Seconds(1), &OranHelper::ActivateAndStartNearRtRic, oranHelper, nearRtRic);
    Simulator::Schedule(Seconds(1.5),
                        &OranHelper::ActivateE2NodeTerminators,
                        oranHelper,
                        gnbTerminators);

    // Traces
    AsciiTraceHelper ascii;
    g_gnbDlTrace = ascii.CreateFileStream(g_prefix + "-gnb-dl.csv");
    *g_gnbDlTrace->GetStream() << "Time_s,CellId,Bytes\n";
    g_summaryTrace = ascii.CreateFileStream(g_prefix + "-timeseries.csv");
    g_gnbLoadTrace = ascii.CreateFileStream(g_prefix + "-gnb-load.csv");
    Ptr<OutputStreamWrapper> gnbPosTrace = ascii.CreateFileStream(g_prefix + "-gnb-positions.csv");
    *gnbPosTrace->GetStream() << "CellId,X_m,Y_m,Z_m\n";
    for (uint32_t i = 0; i < gnbNodes.GetN(); ++i)
    {
        Vector p = gnbNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        *gnbPosTrace->GetStream() << g_cellIds[i] << "," << p.x << "," << p.y << "," << p.z << "\n";
    }
    internetDevices.Get(1)->TraceConnectWithoutContext("MacTx", MakeCallback(&ServerTx));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NrEpcGnbApplication/RxFromS1u",
                    MakeCallback(&GnbRxFromS1u));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverStart",
                                  MakeCallback(&HandoverStart));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverEndOk",
                                  MakeCallback(&HandoverEndOk));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrUeRrc/RadioLinkFailure",
                                  MakeCallback(&RadioLinkFailure));
    Simulator::Schedule(g_interval, &Report);

    std::cout << "Handover " << (handover ? "ENABLED" : "DISABLED") << ", " << numGnbs
              << " gNBs and " << numUes << " UEs, spacing " << gnbSpacing << " m, UE speed " << speed << " m/s, VR rate "
              << appRate << ", burst generator " << burstGeneratorType << std::endl;

    Simulator::Stop(simTime + Seconds(1));
    Simulator::Run();

    // Loss per UE and interval: fragments sent in the interval and never received by the end
    const size_t numIntervals = g_gnbRows.size();
    std::vector<std::vector<uint64_t>> sent(numUes, std::vector<uint64_t>(numIntervals + 1, 0));
    std::vector<std::vector<uint64_t>> lost = sent;
    for (const auto& [key, interval] : g_sentFragments)
    {
        const uint32_t ue = key >> 56;
        if (interval < numIntervals)
        {
            sent[ue][interval]++;
            lost[ue][interval] += g_rxFragments.count(key) ? 0 : 1;
        }
    }
    std::ostream& os = *g_summaryTrace->GetStream();
    os << "Time_s,UeId,UeType,UeX_m,UeY_m,UeZ_m,ServingCell,RxMbps,MeanDelay_ms,SentPackets,"
          "LostPackets,LossPercent\n";
    for (size_t i = 0; i < g_rows.size(); ++i)
    {
        const Row& r = g_rows[i];
        // The rows of interval k are g_rows[k * numUes ... ], covering [t - interval, t).
        const size_t k = i / numUes;
        os << r.t << "," << r.ue << "," << (r.ue == 0 ? "moving" : "static") << "," << r.uePos.x
           << "," << r.uePos.y << "," << r.uePos.z << "," << r.cell << "," << r.rxMbps << ","
           << r.delayMs << "," << sent[r.ue][k] << "," << lost[r.ue][k] << ","
           << (sent[r.ue][k] ? 100.0 * lost[r.ue][k] / sent[r.ue][k] : 0.0) << "\n";
    }
    os.flush();

    std::ostream& gl = *g_gnbLoadTrace->GetStream();
    gl << "Time_s";
    for (uint16_t cell : g_cellIds)
    {
        gl << ",gNB_cell" << cell << "_DlMbps";
    }
    for (uint16_t cell : g_cellIds)
    {
        gl << ",MovingUeRsrp_cell" << cell << "_dBm";
    }
    gl << "\n";
    for (const GnbRow& r : g_gnbRows)
    {
        gl << r.t;
        for (double m : r.gnbMbps)
        {
            gl << "," << m;
        }
        for (double d : r.rsrp)
        {
            gl << "," << d;
        }
        gl << "\n";
    }
    gl.flush();

    std::cout << "Handovers completed: " << g_handoverCount << std::endl;
    for (uint32_t u = 0; u < numUes; ++u)
    {
        std::cout << "UE " << u << (u == 0 ? " (moving)" : " (static)") << ": received "
                  << g_ueTotalRxBytes[u] * 8 / 1e6 << " Mbit" << std::endl;
    }
    Simulator::Destroy();
    return 0;
}
