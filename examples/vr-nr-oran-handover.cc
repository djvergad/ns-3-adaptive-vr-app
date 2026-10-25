// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-nr-oran-handover.cc
 * @brief Downlink VR traffic to a moving UE that is handed over between NR gNBs by an O-RAN xApp.
 *
 * Scenario: a row of gNBs (default 3) and one UE that moves along the row at constant speed.
 * A remote host streams VR traffic (BurstyApplicationServer with the VrBurstGenerator, as in
 * vr-cttc-nr-oran.cc) to the UE through the core network. The Near-RT RIC runs
 * OranLmNr2NrRsrpHandover, which hands the UE over to the strongest gNB. With the default
 * parameters the UE crosses two cell borders, so two handovers happen.
 *
 * Output:
 *  - Each handover is printed on the terminal as it happens (start and completion).
 *  - <prefix>-gnb-dl.csv: one line per downlink packet reaching a gNB from the core network
 *    (time, cell id, bytes). After a handover the packets show up at the new gNB.
 *  - <prefix>-timeseries.csv: per-interval UE position, serving cell, received VR throughput,
 *    mean delay, packet loss, per-gNB downlink throughput and RSRP. <prefix>-gnb-positions.csv
 *    holds the gNB locations. plot-vr-handover.py plots them.
 *
 * Use --outPrefix to name the output files. Use --handover=false to disable the handovers.
 * The UE then stays attached to the first gNB and, as it moves away, the received VR throughput degrades and finally drops to zero.
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

/// Statistics accumulated during one reporting interval.
struct IntervalStats
{
    std::vector<uint64_t> gnbDlBytes; ///< Downlink bytes reaching each gNB from the core network
    uint64_t ueRxBytes{0};            ///< VR bytes received by the UE
    double delaySum{0};               ///< Sum of the VR fragment delays [s]
    uint64_t delayCount{0};           ///< Number of VR fragments in delaySum
};

IntervalStats g_stats;                     ///< Statistics of the current interval
std::map<uint32_t, uint32_t> g_nodeToGnb;  ///< Node id -> gNB index
std::vector<uint16_t> g_cellIds;           ///< Cell id of each gNB
std::vector<double> g_rsrp;                ///< Last RSRP [dBm] per gNB index, NaN if unknown
Ptr<OutputStreamWrapper> g_gnbDlTrace;     ///< Per-packet gNB downlink trace
Ptr<OutputStreamWrapper> g_summaryTrace;   ///< Per-interval summary trace
Ptr<NrUeNetDevice> g_ueDev;                ///< The UE device
Time g_interval = Seconds(1);              ///< Reporting interval
uint32_t g_handoverCount = 0;              ///< Handovers completed so far
double g_totalRxBytes = 0;                 ///< Total VR bytes received by the UE
Time g_stopReporting;                      ///< No reports are produced after this time
std::string g_prefix = "vr-handover";      ///< Prefix of the output files

/// One row of the time series, written at the end of the simulation (loss is known only then).
struct Row
{
    double t;          ///< Time [s]
    Vector uePos;      ///< UE position
    uint16_t cell;     ///< Serving cell
    double rxMbps;     ///< VR throughput received by the UE [Mbps]
    double delayMs;    ///< Mean VR fragment delay [ms]
    std::vector<double> gnbMbps; ///< Per-gNB downlink throughput [Mbps]
    std::vector<double> rsrp;    ///< RSRP per gNB [dBm]
};
std::vector<Row> g_rows;                     ///< Time series
std::map<uint64_t, uint32_t> g_sentFragments; ///< Fragment key -> interval in which it was sent
std::set<uint64_t> g_rxFragments;             ///< Keys of the fragments received by the UE

/// @return a key identifying a VR fragment (burst sequence and fragment sequence).
uint64_t
FragmentKey(const SeqTsSizeFragHeader& h)
{
    return (static_cast<uint64_t>(h.GetSeq()) << 32) | h.GetFragSeq();
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
    p->RemoveHeader(ip);
    p->RemoveHeader(udp);
    p->PeekHeader(frag);
    g_sentFragments.emplace(FragmentKey(frag),
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
    g_stats.gnbDlBytes[it->second] += packet->GetSize();
    *g_gnbDlTrace->GetStream() << Simulator::Now().GetSeconds() << ","
                               << g_cellIds[it->second] << "," << packet->GetSize() << "\n";
}

void
FragmentRx(Ptr<const Packet> fragment,
           const Address& from,
           const Address& to,
           const SeqTsSizeFragHeader& header)
{
    g_rxFragments.insert(FragmentKey(header));
    g_stats.ueRxBytes += fragment->GetSize();
    g_totalRxBytes += fragment->GetSize();
    g_stats.delaySum += (Simulator::Now() - header.GetTs()).GetSeconds();
    g_stats.delayCount++;
}

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
Report(Ptr<MobilityModel> ueMobility)
{
    const double t = Simulator::Now().GetSeconds();
    const double dt = g_interval.GetSeconds();
    uint16_t cell = g_ueDev->GetRrc()->GetCellId();

    std::ostringstream line;
    line << std::fixed << std::setprecision(1) << t << "," << ueMobility->GetPosition().x << ","
         << cell;
    for (double bytes : g_stats.gnbDlBytes)
    {
        line << "," << bytes * 8 / dt / 1e6;
    }
    line << "," << g_stats.ueRxBytes * 8 / dt / 1e6 << ","
         << (g_stats.delayCount ? g_stats.delaySum / g_stats.delayCount * 1000 : 0.0);
    for (double rsrp : g_rsrp)
    {
        line << "," << rsrp;
    }
    Row row;
    row.t = t;
    row.uePos = ueMobility->GetPosition();
    row.cell = cell;
    row.rxMbps = g_stats.ueRxBytes * 8 / dt / 1e6;
    row.delayMs = g_stats.delayCount ? g_stats.delaySum / g_stats.delayCount * 1000 : 0.0;
    for (double bytes : g_stats.gnbDlBytes)
    {
        row.gnbMbps.push_back(bytes * 8 / dt / 1e6);
    }
    row.rsrp = g_rsrp;
    g_rows.push_back(row);
    std::cout << line.str() << std::endl;

    g_stats.gnbDlBytes.assign(g_cellIds.size(), 0);
    g_stats.ueRxBytes = 0;
    g_stats.delaySum = 0;
    g_stats.delayCount = 0;
    if (Simulator::Now() + g_interval <= g_stopReporting)
    {
        Simulator::Schedule(g_interval, &Report, ueMobility);
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
    ueNodes.Create(1);

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
    ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>())
        ->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // X2 between all gNBs (needed for X2 handover) and initial attachment to the first gNB
    nrHelper->AddX2Interface(gnbNodes);
    nrHelper->AttachToGnb(ueDevs.Get(0), gnbDevs.Get(0));

    g_ueDev = ueDevs.Get(0)->GetObject<NrUeNetDevice>();
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        g_nodeToGnb[gnbNodes.Get(i)->GetId()] = i;
        g_cellIds.push_back(gnbDevs.Get(i)->GetObject<NrGnbNetDevice>()->GetCellId());
    }
    g_stats.gnbDlBytes.assign(numGnbs, 0);
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
    Ptr<BurstyApplicationClient> client = DynamicCast<BurstyApplicationClient>(clientApps.Get(0));
    client->SetAttribute("Local", AddressValue(InetSocketAddress(ueIpIface.GetAddress(0), 0)));
    client->SetStartTime(Seconds(1));
    client->SetStopTime(simTime + Seconds(1));
    client->TraceConnectWithoutContext("FragmentRx", MakeCallback(&FragmentRx));

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

    Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
    Ptr<OranReporterNrUeCellInfo> cellInfoReporter = CreateObject<OranReporterNrUeCellInfo>();
    Ptr<OranReporterNrUeRsrpRsrq> rsrpReporter = CreateObject<OranReporterNrUeRsrpRsrq>();
    Ptr<OranE2NodeTerminatorNrUe> ueTerminator = CreateObject<OranE2NodeTerminatorNrUe>();
    locationReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
    cellInfoReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
    rsrpReporter->SetAttribute("Terminator", PointerValue(ueTerminator));
    g_ueDev->GetPhy(0)->TraceConnectWithoutContext(
        "ReportUeMeasurements",
        MakeCallback(&OranReporterNrUeRsrpRsrq::ReportRsrpRsrq, rsrpReporter));
    g_ueDev->GetPhy(0)->TraceConnectWithoutContext("ReportUeMeasurements",
                                                   MakeCallback(&UeMeasurements));
    ueTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
    ueTerminator->SetAttribute("RegistrationIntervalRv",
                               StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
    ueTerminator->SetAttribute("SendIntervalRv",
                               StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    ueTerminator->AddReporter(locationReporter);
    ueTerminator->AddReporter(cellInfoReporter);
    ueTerminator->AddReporter(rsrpReporter);
    ueTerminator->Attach(ueNodes.Get(0));
    Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorNrUe::Activate, ueTerminator);

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
    Ptr<OutputStreamWrapper> gnbPosTrace = ascii.CreateFileStream(g_prefix + "-gnb-positions.csv");
    *gnbPosTrace->GetStream() << "CellId,X_m,Y_m,Z_m\n";
    for (uint32_t i = 0; i < gnbNodes.GetN(); ++i)
    {
        Vector p = gnbNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        *gnbPosTrace->GetStream() << g_cellIds[i] << "," << p.x << "," << p.y << "," << p.z << "\n";
    }
    internetDevices.Get(1)->TraceConnectWithoutContext("MacTx", MakeCallback(&ServerTx));
    std::ostringstream header;
    header << "Time_s,UeX_m,ServingCell";
    for (uint16_t cell : g_cellIds)
    {
        header << ",gNB_cell" << cell << "_DlMbps";
    }
    header << ",UeRxMbps,MeanFragDelay_ms";
    for (uint16_t cell : g_cellIds)
    {
        header << ",Rsrp_cell" << cell << "_dBm";
    }
    std::cout << header.str() << std::endl;

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NrEpcGnbApplication/RxFromS1u",
                    MakeCallback(&GnbRxFromS1u));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverStart",
                                  MakeCallback(&HandoverStart));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverEndOk",
                                  MakeCallback(&HandoverEndOk));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrUeRrc/RadioLinkFailure",
                                  MakeCallback(&RadioLinkFailure));
    Simulator::Schedule(g_interval, &Report, ueMobility);

    std::cout << "Handover " << (handover ? "ENABLED" : "DISABLED") << ", " << numGnbs
              << " gNBs, spacing " << gnbSpacing << " m, UE speed " << speed << " m/s, VR rate "
              << appRate << ", burst generator " << burstGeneratorType << std::endl;

    Simulator::Stop(simTime + Seconds(1));
    Simulator::Run();

    // Loss per interval: fragments sent in the interval and never received by the end of the run
    std::vector<uint64_t> sent(g_rows.size() + 1, 0);
    std::vector<uint64_t> lost(g_rows.size() + 1, 0);
    for (const auto& [key, interval] : g_sentFragments)
    {
        if (interval < sent.size())
        {
            sent[interval]++;
            lost[interval] += g_rxFragments.count(key) ? 0 : 1;
        }
    }
    std::ostream& os = *g_summaryTrace->GetStream();
    os << "Time_s,UeX_m,UeY_m,UeZ_m,ServingCell,RxMbps,MeanDelay_ms,SentPackets,LostPackets,"
          "LossPercent";
    for (uint16_t cell : g_cellIds)
    {
        os << ",gNB_cell" << cell << "_DlMbps";
    }
    for (uint16_t cell : g_cellIds)
    {
        os << ",Rsrp_cell" << cell << "_dBm";
    }
    os << "\n";
    for (size_t i = 0; i < g_rows.size(); ++i)
    {
        const Row& r = g_rows[i];
        // Row i covers [t - interval, t), i.e. the sending interval index i.
        os << r.t << "," << r.uePos.x << "," << r.uePos.y << "," << r.uePos.z << "," << r.cell
           << "," << r.rxMbps << "," << r.delayMs << "," << sent[i] << "," << lost[i] << ","
           << (sent[i] ? 100.0 * lost[i] / sent[i] : 0.0);
        for (double m : r.gnbMbps)
        {
            os << "," << m;
        }
        for (double d : r.rsrp)
        {
            os << "," << d;
        }
        os << "\n";
    }
    os.flush();

    std::cout << "Handovers completed: " << g_handoverCount << ", total VR data received by the UE: "
              << g_totalRxBytes * 8 / 1e6 << " Mbit" << std::endl;
    Simulator::Destroy();
    return 0;
}
