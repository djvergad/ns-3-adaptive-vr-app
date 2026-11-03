// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef VR_CTTC_NR_ORAN_SCENARIO_H
#define VR_CTTC_NR_ORAN_SCENARIO_H

/**
 * @file vr-cttc-nr-oran-scenario.h
 * @brief VR over NR with O-RAN: static UEs around a single gNB (based on cttc-nr-demo).
 *
 * A remote (edge) host streams VR traffic (BurstyApplicationServer with the VrBurstGenerator)
 * to the UEs through the core network; the VR bitrate adaptation algorithm is selected with
 * --burstGeneratorType. A Near-RT RIC collects E2 reports from the UEs and the gNB; it runs
 * no handover logic (there is a single cell). The VR application, transport and O-RAN
 * settings are the common ones of vr-nr-oran-common.h.
 *
 * The UEs are placed either by GridScenarioHelper in a 3 x 3 m area next to the gNB
 * (--uePlacement=grid) or on three rings around it (--uePlacement=rings): a third of them at
 * 2 m, a third at --mediumUeDistance and the rest at --farUeDistance.
 *
 * Every UE starts its stream at a random time in [2.5, 3] s and receives --simulationTime of
 * VR traffic. The simulation ends 1 s after the last stream, so that the last frames can
 * arrive.
 *
 * Programs: vr-cttc-nr-oran.cc (grid defaults) and vr-cttc-nr-oran-distance.cc (rings
 * defaults). Every default can be overridden from the command line; see --PrintHelp.
 *
 * Output (in the working directory):
 *  - burstTrace.csv, fragmentTrace.csv: bursts/fragments received by the UEs, with the UE
 *    distance from the gNB.
 *  - txFragmentTrace.csv: fragments sent by the server.
 *  - txBurstsBySta.csv, rxBursts.csv, txFragmentsBySta.csv, rxFragments.csv: totals.
 *  - <outputDir>/<simTag>: per-flow statistics from the flow monitor, sorted by UE.
 */

#include "vr-nr-oran-common.h"

#include "ns3/antenna-module.h"
#include "ns3/bursty-application-server-instance.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"

#include <cmath>
#include <fstream>
#include <map>

namespace ns3
{
namespace vrnr
{

/// Options of the CTTC NR O-RAN VR scenario. The programs set the defaults.
struct CttcNrOranOptions
{
    // Scenario
    uint16_t ueNumPergNb{2};         ///< Number of UEs
    bool logging{false};             ///< Enable logging of some components
    std::string uePlacement{"grid"}; ///< UE placement {"grid", "rings"}
    double mediumUeDistance{30.0};   ///< Distance of the medium ring from the gNB [m]
    double farUeDistance{50.0};      ///< Distance of the far ring from the gNB [m]
    Time simTime{Seconds(10)};       ///< Duration of each UE's VR stream

    // Channel
    std::string channelScenario{"UMi"};        ///< 3GPP scenario {"UMi", "UMa", "RMa"}
    bool enableShadowing{false};               ///< Enable shadowing
    Time channelUpdatePeriod{MilliSeconds(0)}; ///< Channel condition update period

    // NR: one band with one CC and one BWP
    double centralFrequency{28e9}; ///< Central frequency [Hz]
    double bandwidth{50e6};        ///< Bandwidth [Hz]
    uint16_t numerology{2};        ///< Numerology
    double txPower{35};            ///< gNB TX power [dBm]

    // VR application, transport and O-RAN
    VrOranOptions vr; ///< Common options

    // Output
    std::string simTag{"default"}; ///< Name of the flow statistics file
    std::string outputDir{"./"};   ///< Directory of the flow statistics file
};

/// Distance [m] of each UE address from the gNB.
using UeDistanceMap = std::map<Ipv4Address, float>;

/**
 * @param addr An address.
 * @return The IPv4 address in addr as a string.
 */
inline std::string
AddressToString(const Address& addr)
{
    std::stringstream addressStr;
    if (InetSocketAddress::IsMatchingType(addr))
    {
        addressStr << InetSocketAddress::ConvertFrom(addr).GetIpv4();
    }
    else if (Ipv4Address::IsMatchingType(addr))
    {
        addressStr << Ipv4Address::ConvertFrom(addr);
    }
    else
    {
        addressStr << addr;
    }
    return addressStr.str();
}

/**
 * @param distances The UE distances.
 * @param ue The UE address.
 * @return The distance of the UE from the gNB [m], or -1 if unknown.
 */
inline float
UeDistance(const UeDistanceMap* distances, const Address& ue)
{
    Ipv4Address ip = InetSocketAddress::IsMatchingType(ue)
                         ? InetSocketAddress::ConvertFrom(ue).GetIpv4()
                         : Ipv4Address::ConvertFrom(ue);
    auto it = distances->find(ip);
    return it != distances->end() ? it->second : -1.0f;
}

/**
 * BurstRx trace sink.
 *
 * @param traceFile The trace file.
 * @param distances The UE distances.
 * @param burst The burst.
 * @param from The source address.
 * @param to The destination address.
 * @param header The burst header.
 */
inline void
BurstRx(Ptr<OutputStreamWrapper> traceFile,
        const UeDistanceMap* distances,
        Ptr<const Packet> burst,
        const Address& from,
        const Address& to,
        const SeqTsSizeFragHeader& header)
{
    *traceFile->GetStream() << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
                            << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << ","
                            << header.GetSize() << "," << UeDistance(distances, to) << "\n";
}

/**
 * FragmentRx trace sink.
 *
 * @param traceFile The trace file.
 * @param distances The UE distances.
 * @param fragment The fragment.
 * @param from The source address.
 * @param to The destination address.
 * @param header The fragment header.
 */
inline void
FragmentRx(Ptr<OutputStreamWrapper> traceFile,
           const UeDistanceMap* distances,
           Ptr<const Packet> fragment,
           const Address& from,
           const Address& to,
           const SeqTsSizeFragHeader& header)
{
    *traceFile->GetStream() << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
                            << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << ","
                            << header.GetFragSeq() << "," << header.GetFrags() << ","
                            << fragment->GetSize() << "," << UeDistance(distances, to) << "\n";
}

/**
 * Write the flow monitor statistics sorted by destination address. The throughput of a flow
 * is computed between its first transmitted and its last received packet.
 *
 * @param stats The flow statistics.
 * @param classifier The flow classifier.
 * @param outFile The output file.
 */
inline void
PrintFlowStats(const FlowMonitor::FlowStatsContainer& stats,
               Ptr<Ipv4FlowClassifier> classifier,
               std::ofstream& outFile)
{
    std::vector<const FlowMonitor::FlowStatsContainer::value_type*> sorted;
    sorted.reserve(stats.size());
    for (const auto& pair : stats)
    {
        sorted.push_back(&pair);
    }
    std::ranges::sort(sorted, {}, [classifier](const auto* pair) {
        return classifier->FindFlow(pair->first).destinationAddress.Get();
    });

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;
    for (const auto* pair : sorted)
    {
        const auto& st = pair->second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(pair->first);

        outFile << "Flow " << pair->first << " (" << t.sourceAddress << " -> "
                << t.destinationAddress << ")\n"
                << "  Tx Packets: " << st.txPackets << "\n"
                << "  Rx Packets: " << st.rxPackets << "\n"
                << "  Rx Bytes:   " << st.rxBytes << "\n";

        if (st.rxPackets > 0)
        {
            double flowThroughput = st.rxBytes * 8.0 /
                                    (st.timeLastRxPacket - st.timeFirstTxPacket).GetSeconds() /
                                    1000 / 1000; // Mbps
            double flowDelay = 1000 * st.delaySum.GetSeconds() / st.rxPackets;
            averageFlowThroughput += flowThroughput;
            averageFlowDelay += flowDelay;
            outFile << "  Throughput: " << flowThroughput << " Mbps\n";
            outFile << "  Mean delay:  " << flowDelay << " ms\n";
            outFile << "  Mean jitter:  " << 1000 * st.jitterSum.GetSeconds() / st.rxPackets
                    << " ms\n";
        }
    }

    outFile << "\n\n  Mean flow throughput: " << averageFlowThroughput / sorted.size() << "\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / sorted.size() << "\n";
}

/**
 * Place the UEs on three rings around a gNB: a third at 2 m, a third at mediumDistance and
 * the rest at farDistance, evenly spaced on each ring, at 1.5 m height.
 *
 * @param ues The UEs.
 * @param gnbPos The gNB position.
 * @param mediumDistance Radius of the medium ring [m].
 * @param farDistance Radius of the far ring [m].
 */
inline void
PlaceUesOnRings(const NodeContainer& ues, Vector gnbPos, double mediumDistance, double farDistance)
{
    const uint32_t nearUes = ues.GetN() / 3;
    const uint32_t mediumUes = ues.GetN() / 3;
    const uint32_t farUes = ues.GetN() - nearUes - mediumUes;
    const std::vector<std::pair<uint32_t, double>> rings = {{nearUes, 2.0},
                                                            {mediumUes, mediumDistance},
                                                            {farUes, farDistance}};

    uint32_t first = 0;
    for (const auto& [count, radius] : rings)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            double angle = (2.0 * M_PI * i) / count;
            Vector pos(gnbPos.x + radius * cos(angle), gnbPos.y + radius * sin(angle), 1.5);
            ues.Get(first + i)->GetObject<MobilityModel>()->SetPosition(pos);
        }
        first += count;
    }
}

/**
 * Run the scenario.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments.
 * @param program The program file, for the command line usage.
 * @param o The default options.
 * @return The program exit code.
 */
inline int
RunCttcNrOran(int argc, char* argv[], const std::string& program, CttcNrOranOptions o)
{
    CommandLine cmd(program);
    cmd.AddValue("ueNumPergNb", "Number of UEs", o.ueNumPergNb);
    cmd.AddValue("logging", "Enable logging", o.logging);
    cmd.AddValue("uePlacement",
                 "UE placement {\"grid\" (GridScenarioHelper, 3 x 3 m), \"rings\" (a third of "
                 "the UEs at 2 m, mediumUeDistance and farUeDistance from the gNB)}",
                 o.uePlacement);
    cmd.AddValue("mediumUeDistance",
                 "rings: distance of the medium UEs from the gNB [m] (negative: farUeDistance/2)",
                 o.mediumUeDistance);
    cmd.AddValue("farUeDistance",
                 "rings: distance of the far UEs from the gNB [m]",
                 o.farUeDistance);
    cmd.AddValue("simulationTime", "Duration of the VR stream of each UE", o.simTime);
    cmd.AddValue(
        "channelScenario",
        "3GPP channel scenario: UMi (Urban Micro), UMa (Urban Macro), or RMa (Rural Macro)",
        o.channelScenario);
    cmd.AddValue("enableShadowing", "Enable shadowing in the channel model", o.enableShadowing);
    cmd.AddValue("channelUpdatePeriod",
                 "Channel condition update period (0: never updated)",
                 o.channelUpdatePeriod);
    cmd.AddValue("frequency", "Central frequency [Hz]", o.centralFrequency);
    cmd.AddValue("bandwidth", "Bandwidth [Hz]", o.bandwidth);
    cmd.AddValue("numerology", "NR numerology", o.numerology);
    cmd.AddValue("txPower", "gNB TX power [dBm]", o.txPower);
    o.vr.AddCommandLineArgs(cmd);
    cmd.AddValue("simTag", "Name of the flow statistics file", o.simTag);
    cmd.AddValue("outputDir", "Directory of the flow statistics file", o.outputDir);
    cmd.Parse(argc, argv);

    NS_ABORT_IF(o.centralFrequency < 0.5e9 || o.centralFrequency > 100e9);
    NS_ABORT_MSG_IF(
        o.channelScenario != "UMi" && o.channelScenario != "UMa" && o.channelScenario != "RMa",
        "Invalid channel scenario: " << o.channelScenario << ". Valid options are: UMi, UMa, RMa");
    NS_ABORT_MSG_IF(o.uePlacement != "grid" && o.uePlacement != "rings",
                    "Invalid uePlacement: " << o.uePlacement);
    const bool rings = o.uePlacement == "rings";
    NS_ABORT_MSG_IF(rings && o.farUeDistance < 10.0, "farUeDistance must be at least 10 m");
    if (o.mediumUeDistance < 0.0)
    {
        o.mediumUeDistance = o.farUeDistance / 2.0;
    }

    // Each stream starts at a random time in [startMin, startMax] and lasts simTime
    const Time startMin = MilliSeconds(2500);
    const Time startMax = Seconds(3);
    const Time stopTime = startMax + o.simTime + Seconds(1); // drain the last frames

    // Logging can also be enabled with the NS_LOG environment variable
    if (o.logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
    }
    LogComponentEnableAll(LOG_PREFIX_ALL);

    ConfigureCommonDefaults(o.vr, o.simTime);

    /*
     * Create the scenario. GridScenarioHelper places the gNB and the UEs (randomly in a
     * 3 x 3 m area); with the rings placement the UEs are then moved to the rings.
     */
    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(1);
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(1);
    gridScenario.SetUtNumber(o.ueNumPergNb);
    const double scenarioSize = rings ? std::max(o.farUeDistance * 2.5, 100.0) : 3.0;
    gridScenario.SetScenarioHeight(scenarioSize);
    gridScenario.SetScenarioLength(scenarioSize);
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    NodeContainer gnbNodes = gridScenario.GetBaseStations();
    NodeContainer ueNodes = gridScenario.GetUserTerminals();
    const Vector gnbPos = gnbNodes.Get(0)->GetObject<MobilityModel>()->GetPosition();
    if (rings)
    {
        PlaceUesOnRings(ueNodes, gnbPos, o.mediumUeDistance, o.farUeDistance);
    }

    NrHelpers nr = CreateNrHelpers(o.vr);

    // Spectrum: one operational band with one CC containing one BWP
    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(o.centralFrequency, o.bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories(o.channelScenario, "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod",
                                                     TimeValue(o.channelUpdatePeriod));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(o.enableShadowing));
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    // mmWave antenna arrays
    nr.nr->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nr.nr->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nr.nr->SetUeAntennaAttribute("AntennaElement",
                                 PointerValue(CreateObject<IsotropicAntennaModel>()));
    nr.nr->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nr.nr->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nr.nr->SetGnbAntennaAttribute("AntennaElement",
                                  PointerValue(CreateObject<IsotropicAntennaModel>()));

    NetDeviceContainer gnbDevs = nr.nr->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nr.nr->InstallUeDevice(ueNodes, allBwps);
    randomStream += nr.nr->AssignStreams(gnbDevs, randomStream);
    randomStream += nr.nr->AssignStreams(ueDevs, randomStream);

    NrHelper::GetGnbPhy(gnbDevs.Get(0), 0)->SetAttribute("Numerology", UintegerValue(o.numerology));
    NrHelper::GetGnbPhy(gnbDevs.Get(0), 0)->SetAttribute("TxPower", DoubleValue(o.txPower));
    SetUeTxPower(ueDevs, o.vr.ueTxPower);

    RemoteHost remoteHost = CreateRemoteHost(nr.epc, o.vr);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = nr.epc->AssignUeIpv4Address(ueDevs);
    SetUeDefaultRoutes(ueNodes, nr.epc);
    nr.nr->AttachToClosestGnb(ueDevs, gnbDevs);
    ActivateVrQosFlow(nr.nr, ueDevs, o.vr);

    UeDistanceMap ueDistances;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Vector pos = ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        ueDistances[ueIpIface.GetAddress(i)] = static_cast<float>(CalculateDistance(gnbPos, pos));
    }

    // O-RAN: the RIC is initialized before the applications, so that the VR bitrate logic
    // module has its data repository when the adaptation algorithms are created. There is a
    // single cell, so the RIC runs no handover logic.
    Ptr<OranLogicVrBitrate> oranLogicVrBitrate = CreateObject<OranLogicVrBitrate>();
    const BurstGeneratorSetup generator =
        ConfigureBurstGenerator(o.vr.burstGeneratorType, oranLogicVrBitrate);
    Ptr<OranHelper> oranHelper = CreateOranHelper(o.vr);
    oranHelper->SetDefaultLogicModule("ns3::OranLmNoop");
    OranDeployment oran;
    DeployOran(oranHelper,
               o.vr,
               generator,
               oranLogicVrBitrate,
               nr.epc->GetPgwNode(),
               remoteHost.node,
               gnbNodes,
               gnbDevs,
               ueNodes,
               stopTime,
               oran);

    // The VR traffic: server on the remote host, one client per UE
    ApplicationContainer serverApp =
        InstallVrServer(generator.socketFactory, remoteHost.node, stopTime);
    ApplicationContainer clientApps =
        InstallVrClients(generator.socketFactory, remoteHost.address, ueNodes, ueIpIface, stopTime);
    Ptr<UniformRandomVariable> randomStart =
        CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                          DoubleValue(startMin.GetSeconds()),
                                                          "Max",
                                                          DoubleValue(startMax.GetSeconds()));

    // Traces
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> burstTrace = ascii.CreateFileStream("burstTrace.csv");
    *burstTrace->GetStream() << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,BurstSize,Distance"
                             << std::endl;
    Ptr<OutputStreamWrapper> fragmentTrace = ascii.CreateFileStream("fragmentTrace.csv");
    *fragmentTrace->GetStream()
        << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize,Distance"
        << std::endl;

    // Fragments sent by the server, to correlate the sent and the received fragments
    Ptr<OutputStreamWrapper> txFragmentTrace = ascii.CreateFileStream("txFragmentTrace.csv");
    *txFragmentTrace->GetStream()
        << "DstAddress,EventTime_ns,TxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize,Distance"
        << std::endl;
    serverApp.Get(0)->TraceConnectWithoutContext(
        "FragmentRx",
        MakeBoundCallback(&FragmentRx, txFragmentTrace, &ueDistances));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Time startTime = Seconds(randomStart->GetValue());
        NS_LOG_UNCOND("STA" << i << " will start at " << startTime.As(Time::S));
        Ptr<Application> app = clientApps.Get(i);
        app->SetStartTime(startTime);
        app->TraceConnectWithoutContext("BurstRx",
                                        MakeBoundCallback(&BurstRx, burstTrace, &ueDistances));
        app->TraceConnectWithoutContext(
            "FragmentRx",
            MakeBoundCallback(&FragmentRx, fragmentTrace, &ueDistances));
    }

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost.node);
    endpointNodes.Add(ueNodes);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(stopTime);
    Simulator::Run();

    // Per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream outFile;
    std::string filename = o.outputDir + "/" + o.simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }
    outFile.setf(std::ios_base::fixed);
    PrintFlowStats(stats, classifier, outFile);
    outFile.close();

    std::ifstream f(filename.c_str());
    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    // Bursts and fragments sent and received
    Ptr<BurstyApplicationServer> server = DynamicCast<BurstyApplicationServer>(serverApp.Get(0));
    Ptr<OutputStreamWrapper> txBurstsBySta = ascii.CreateFileStream("txBurstsBySta.csv");
    Ptr<OutputStreamWrapper> rxBursts = ascii.CreateFileStream("rxBursts.csv");
    uint64_t totBurstSent = 0;
    for (auto& kv : server->GetInstances())
    {
        uint64_t burstsSent = kv.second.GetTotalTxBursts();
        totBurstSent += burstsSent;
        std::cout << "burstsSent(" << kv.first << ")=" << burstsSent << ", ";
        *txBurstsBySta->GetStream() << burstsSent << std::endl;
    }
    uint64_t burstsReceived = 0;
    for (uint32_t i = 0; i < clientApps.GetN(); i++)
    {
        burstsReceived +=
            DynamicCast<BurstyApplicationClient>(clientApps.Get(i))->GetTotalRxBursts();
    }
    std::cout << "burstsReceived=" << burstsReceived << " ("
              << double(burstsReceived) / totBurstSent * 100 << "%)" << std::endl;
    *rxBursts->GetStream() << burstsReceived << std::endl;

    Ptr<OutputStreamWrapper> txFragmentsBySta = ascii.CreateFileStream("txFragmentsBySta.csv");
    Ptr<OutputStreamWrapper> rxFragments = ascii.CreateFileStream("rxFragments.csv");
    uint64_t totFragmentSent = 0;
    for (auto& kv : server->GetInstances())
    {
        uint64_t fragmentsSent = kv.second.GetTotalTxFragments();
        totFragmentSent += fragmentsSent;
        std::cout << "fragmentsSent(" << kv.first << ")=" << fragmentsSent << ", ";
        *txFragmentsBySta->GetStream() << fragmentsSent << std::endl;
    }
    uint64_t fragmentsReceived = 0;
    for (uint32_t i = 0; i < clientApps.GetN(); i++)
    {
        fragmentsReceived +=
            DynamicCast<BurstyApplicationClient>(clientApps.Get(i))->GetTotalRxFragments();
    }
    std::cout << "fragmentsReceived=" << fragmentsReceived << " ("
              << double(fragmentsReceived) / totFragmentSent * 100 << "%)" << std::endl;
    *rxFragments->GetStream() << fragmentsReceived << std::endl;

    oran.e2Stats.Print(std::cout);
    Simulator::Destroy();
    return 0;
}

} // namespace vrnr
} // namespace ns3

#endif // VR_CTTC_NR_ORAN_SCENARIO_H
