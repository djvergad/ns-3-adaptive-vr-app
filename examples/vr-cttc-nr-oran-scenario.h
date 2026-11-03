// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef VR_CTTC_NR_ORAN_SCENARIO_H
#define VR_CTTC_NR_ORAN_SCENARIO_H

/**
 * @file vr-cttc-nr-oran-scenario.h
 * @brief VR over NR with O-RAN, static UEs around the gNBs (based on cttc-nr-demo).
 *
 * A remote host streams VR traffic (BurstyApplicationServer with the VrBurstGenerator) to the
 * UEs through the core network; the VR bitrate adaptation algorithm is selected with
 * --burstGeneratorType. A Near-RT RIC collects E2 reports from the UEs and gNBs.
 *
 * The UEs are placed either by GridScenarioHelper (--uePlacement=grid) or on three rings
 * around the first gNB (--uePlacement=rings): a third of them at 2 m, a third at
 * --mediumUeDistance and the rest at --farUeDistance.
 *
 * Programs: vr-cttc-nr-oran.cc (grid defaults) and vr-cttc-nr-oran-distance.cc (rings
 * defaults). Every default can be overridden from the command line; see --PrintHelp.
 *
 * Output (in the working directory):
 *  - burstTrace.csv, fragmentTrace.csv: bursts/fragments received by the UEs.
 *  - txFragmentTrace.csv: fragments sent by the server.
 *  - txBurstsBySta.csv, rxBursts.csv, txFragmentsBySta.csv, rxFragments.csv: totals.
 *  - <outputDir>/<simTag>: per-flow statistics from the flow monitor.
 * With --traceUeDistance the per-packet traces have a Distance column (UE to first gNB).
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
    uint16_t gNbNum{1};               ///< Number of gNBs
    uint16_t ueNumPergNb{2};          ///< Number of UEs per gNB
    bool logging{false};              ///< Enable logging of some components
    std::string uePlacement{"grid"};  ///< UE placement {"grid", "rings"}
    double mediumUeDistance{30.0};    ///< Distance of the medium ring from the gNB [m]
    double farUeDistance{50.0};       ///< Distance of the far ring from the gNB [m]
    Time simTime{MilliSeconds(1000)}; ///< Simulation time

    // Channel
    std::string channelScenario{"UMi"};        ///< 3GPP scenario {"UMi", "UMa", "RMa"}
    bool enableShadowing{false};               ///< Enable shadowing
    Time channelUpdatePeriod{MilliSeconds(0)}; ///< Channel condition update period

    // NR
    bool doubleOperationalBand{true};        ///< Two bands with one CC and one BWP each
    uint16_t numerologyBwp1{4};              ///< Numerology of BWP 1
    double centralFrequencyBand1{28e9};      ///< Central frequency of band 1 [Hz]
    double bandwidthBand1{50e6};             ///< Bandwidth of band 1 [Hz]
    uint16_t numerologyBwp2{2};              ///< Numerology of BWP 2
    double centralFrequencyBand2{28.2e9};    ///< Central frequency of band 2 [Hz]
    double bandwidthBand2{50e6};             ///< Bandwidth of band 2 [Hz]
    double totalTxPower{35};                 ///< gNB total TX power [dBm], split among the BWPs
    std::string vrQosFlow{"GBR_CONV_VOICE"}; ///< 5QI of the dedicated flow carrying the VR traffic
    uint32_t vrBwp{1}; ///< BWP of the VR flow (0 when doubleOperationalBand is false)

    // VR application and O-RAN
    VrOranOptions vr;    ///< VR application and O-RAN options
    bool e2Trace{false}; ///< Print every E2 message sent/received over the network

    // Output
    bool traceUeDistance{false};   ///< Add the UE distance from the first gNB to the traces
    bool sortFlowStats{false};     ///< Sort the flow statistics by destination address
    std::string simTag{"default"}; ///< Name of the flow statistics file
    std::string outputDir{"./"};   ///< Directory of the flow statistics file
};

/// Distance [m] of each UE address from the first gNB.
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
 * Append ",<distance>" to a trace line if the distances are traced.
 *
 * @param os The trace stream.
 * @param distances The UE distances, or nullptr.
 * @param ue The UE address.
 */
inline void
WriteUeDistance(std::ostream& os, const UeDistanceMap* distances, const Address& ue)
{
    if (!distances)
    {
        return;
    }
    Ipv4Address ip = InetSocketAddress::IsMatchingType(ue)
                         ? InetSocketAddress::ConvertFrom(ue).GetIpv4()
                         : Ipv4Address::ConvertFrom(ue);
    auto it = distances->find(ip);
    os << "," << (it != distances->end() ? it->second : -1.0f);
}

/**
 * BurstRx trace sink.
 *
 * @param traceFile The trace file.
 * @param distances The UE distances, or nullptr.
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
    std::ostream& os = *traceFile->GetStream();
    os << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
       << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << "," << header.GetSize();
    WriteUeDistance(os, distances, to);
    os << "\n";
}

/**
 * FragmentRx trace sink.
 *
 * @param traceFile The trace file.
 * @param distances The UE distances, or nullptr.
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
    std::ostream& os = *traceFile->GetStream();
    os << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
       << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << "," << header.GetFragSeq()
       << "," << header.GetFrags() << "," << fragment->GetSize();
    WriteUeDistance(os, distances, to);
    os << "\n";
}

/**
 * Write the flow monitor statistics in flow id order. The throughput is computed over
 * the whole flow duration.
 *
 * @param stats The flow statistics.
 * @param classifier The flow classifier.
 * @param flowDuration The flow duration used for the throughput [s].
 * @param outFile The output file.
 */
inline void
PrintFlowStats(const FlowMonitor::FlowStatsContainer& stats,
               Ptr<Ipv4FlowClassifier> classifier,
               double flowDuration,
               std::ofstream& outFile)
{
    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto "
                << protoStream.str() << "\n";
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
        outFile << "  TxOffered:  " << i->second.txBytes * 8.0 / flowDuration / 1000.0 / 1000.0
                << " Mbps\n";
        outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        if (i->second.rxPackets > 0)
        {
            averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            outFile << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
        }
        else
        {
            outFile << "  Throughput:  0 Mbps\n";
            outFile << "  Mean delay:  0 ms\n";
            outFile << "  Mean jitter: 0 ms\n";
        }
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
    }

    outFile << "\n\n  Mean flow throughput: " << averageFlowThroughput / stats.size() << "\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "\n";
}

/**
 * Write the flow monitor statistics sorted by destination address. The throughput is
 * computed between the first transmitted and the last received packet of each flow.
 *
 * @param stats The flow statistics.
 * @param classifier The flow classifier.
 * @param outFile The output file.
 */
inline void
PrintSortedFlowStats(const FlowMonitor::FlowStatsContainer& stats,
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
 * @param name A 5QI name as used by the BWP manager attributes (e.g. "GBR_CONV_VOICE").
 * @return The corresponding 5QI.
 */
inline NrQosFlow::FiveQi
QosFlowFromName(const std::string& name)
{
    static const std::map<std::string, NrQosFlow::FiveQi> flows = {
        {"GBR_CONV_VOICE", NrQosFlow::GBR_CONV_VOICE},
        {"GBR_CONV_VIDEO", NrQosFlow::GBR_CONV_VIDEO},
        {"GBR_NON_CONV_VIDEO", NrQosFlow::GBR_NON_CONV_VIDEO},
        {"NGBR_VIDEO_TCP_DEFAULT", NrQosFlow::NGBR_VIDEO_TCP_DEFAULT},
        {"NGBR_LOW_LAT_EMBB", NrQosFlow::NGBR_LOW_LAT_EMBB},
    };
    auto it = flows.find(name);
    NS_ABORT_MSG_IF(it == flows.end(), "Unsupported vrQosFlow: " << name);
    return it->second;
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
    cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology", o.gNbNum);
    cmd.AddValue("ueNumPergNb", "The number of UE per gNb in multiple-ue topology", o.ueNumPergNb);
    cmd.AddValue("logging", "Enable logging", o.logging);
    cmd.AddValue("uePlacement",
                 "UE placement {\"grid\" (GridScenarioHelper), \"rings\" (a third of the UEs "
                 "at 2 m, mediumUeDistance and farUeDistance from the first gNB)}",
                 o.uePlacement);
    cmd.AddValue("mediumUeDistance",
                 "rings: distance of the medium UEs from the gNB [m] (negative: farUeDistance/2)",
                 o.mediumUeDistance);
    cmd.AddValue("farUeDistance",
                 "rings: distance of the far UEs from the gNB [m]",
                 o.farUeDistance);
    cmd.AddValue("simulationTime", "Simulation time", o.simTime);
    cmd.AddValue(
        "channelScenario",
        "3GPP channel scenario: UMi (Urban Micro), UMa (Urban Macro), or RMa (Rural Macro)",
        o.channelScenario);
    cmd.AddValue("enableShadowing", "Enable shadowing in the channel model", o.enableShadowing);
    cmd.AddValue("channelUpdatePeriod",
                 "Channel condition update period (0: never updated)",
                 o.channelUpdatePeriod);
    cmd.AddValue("doubleOperationalBand",
                 "If true, simulate two operational bands with one CC for each band,"
                 "and each CC will have 1 BWP that spans the entire CC.",
                 o.doubleOperationalBand);
    cmd.AddValue("numerologyBwp1",
                 "The numerology to be used in bandwidth part 1",
                 o.numerologyBwp1);
    cmd.AddValue("centralFrequencyBand1",
                 "The system frequency to be used in band 1",
                 o.centralFrequencyBand1);
    cmd.AddValue("bandwidthBand1", "The system bandwidth to be used in band 1", o.bandwidthBand1);
    cmd.AddValue("numerologyBwp2",
                 "The numerology to be used in bandwidth part 2",
                 o.numerologyBwp2);
    cmd.AddValue("centralFrequencyBand2",
                 "The system frequency to be used in band 2",
                 o.centralFrequencyBand2);
    cmd.AddValue("bandwidthBand2", "The system bandwidth to be used in band 2", o.bandwidthBand2);
    cmd.AddValue("totalTxPower",
                 "total tx power that will be proportionally assigned to"
                 " bands, CCs and bandwidth parts depending on each BWP bandwidth ",
                 o.totalTxPower);
    cmd.AddValue("vrQosFlow",
                 "5QI of the dedicated QoS flow carrying the VR traffic {GBR_CONV_VOICE, "
                 "GBR_CONV_VIDEO, GBR_NON_CONV_VIDEO, NGBR_VIDEO_TCP_DEFAULT, NGBR_LOW_LAT_EMBB}",
                 o.vrQosFlow);
    cmd.AddValue("vrBwp",
                 "Bandwidth part of the VR QoS flow (0 if doubleOperationalBand is false)",
                 o.vrBwp);
    o.vr.AddCommandLineArgs(cmd);
    cmd.AddValue("e2Trace", "Print every E2 message sent/received over the network", o.e2Trace);
    cmd.AddValue("traceUeDistance",
                 "Add the UE distance from the first gNB to the burst and fragment traces",
                 o.traceUeDistance);
    cmd.AddValue("sortFlowStats",
                 "Sort the flow statistics by destination, with the throughput computed "
                 "between the first sent and the last received packet",
                 o.sortFlowStats);
    cmd.AddValue("simTag",
                 "tag to be appended to output filenames to distinguish simulation campaigns",
                 o.simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", o.outputDir);
    cmd.Parse(argc, argv);

    NS_ABORT_IF(o.centralFrequencyBand1 < 0.5e9 || o.centralFrequencyBand1 > 100e9);
    NS_ABORT_IF(o.centralFrequencyBand2 < 0.5e9 || o.centralFrequencyBand2 > 100e9);
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
    const NrQosFlow::FiveQi vrQci = QosFlowFromName(o.vrQosFlow);
    const uint32_t vrBwp = o.doubleOperationalBand ? o.vrBwp : 0;
    NS_ABORT_MSG_IF(vrBwp > 1, "vrBwp must be 0 or 1");

    // Logging can also be enabled with the NS_LOG environment variable
    if (o.logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
    }
    LogComponentEnableAll(LOG_PREFIX_ALL);

    // Very large RLC buffers, so that the VR traffic is not dropped at the gNB
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::NrRlcAm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpBbr")));

    /*
     * Create the scenario. GridScenarioHelper places the gNBs on a row (10 m apart) and the
     * UEs randomly; with the rings placement the UEs are then moved to the rings.
     */
    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(o.gNbNum);
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(o.gNbNum);
    gridScenario.SetUtNumber(o.ueNumPergNb * o.gNbNum);
    const double scenarioSize = rings ? std::max(o.farUeDistance * 2.5, 100.0) : 3.0;
    gridScenario.SetScenarioHeight(scenarioSize);
    gridScenario.SetScenarioLength(scenarioSize);
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    NodeContainer ueNodes = gridScenario.GetUserTerminals();
    const Vector gnbPos =
        gridScenario.GetBaseStations().Get(0)->GetObject<MobilityModel>()->GetPosition();
    if (rings)
    {
        PlaceUesOnRings(ueNodes, gnbPos, o.mediumUeDistance, o.farUeDistance);
    }

    /*
     * NR helpers: the EPC helper sets up the core network, the beamforming helper the
     * beams, the NR helper the NR stack and the channel helper the spectrum channel.
     */
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));

    /*
     * Spectrum: one or two operational bands, each with one CC containing one BWP.
     * ------------Band1--------------|--------------Band2-----------------
     * ------------CC1----------------|--------------CC2-------------------
     * ------------BWP1---------------|--------------BWP2------------------
     */
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf1(o.centralFrequencyBand1,
                                                    o.bandwidthBand1,
                                                    numCcPerBand);
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    CcBwpCreator::SimpleOperationBandConf bandConf2(o.centralFrequencyBand2,
                                                    o.bandwidthBand2,
                                                    numCcPerBand);
    OperationBandInfo band2 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf2);

    // The total power is split among the BWPs proportionally to their bandwidth
    double x = pow(10, o.totalTxPower / 10);
    double totalBandwidth = o.bandwidthBand1;

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories(o.channelScenario, "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod",
                                                     TimeValue(o.channelUpdatePeriod));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(o.enableShadowing));
    if (o.doubleOperationalBand)
    {
        channelHelper->AssignChannelsToBands({band1, band2});
        totalBandwidth += o.bandwidthBand2;
        allBwps = CcBwpCreator::GetAllBwps({band1, band2});
    }
    else
    {
        channelHelper->AssignChannelsToBands({band1});
        allBwps = CcBwpCreator::GetAllBwps({band1});
    }

    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Route the VR QoS flow to its bandwidth part
    nrHelper->SetGnbBwpManagerAlgorithmAttribute(o.vrQosFlow, UintegerValue(vrBwp));
    nrHelper->SetUeBwpManagerAlgorithmAttribute(o.vrQosFlow, UintegerValue(vrBwp));

    NetDeviceContainer gnbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);
    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    // Per-BWP attributes of the first gNB
    NrHelper::GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(o.numerologyBwp1));
    NrHelper::GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((o.bandwidthBand1 / totalBandwidth) * x)));
    if (o.doubleOperationalBand)
    {
        NrHelper::GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetAttribute("Numerology", UintegerValue(o.numerologyBwp2));
        NrHelper::GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetTxPower(10 * log10((o.bandwidthBand2 / totalBandwidth) * x));
    }

    auto [remoteHost, remoteHostIpv4Address] =
        nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.000));

    // The first non-loopback address of the remote host
    Ptr<Ipv4> remoteHostIpv4 = remoteHost->GetObject<Ipv4>();
    Ipv4Address remoteHostActualAddress = remoteHostIpv4Address;
    for (uint32_t i = 1; i < remoteHostIpv4->GetNInterfaces(); ++i)
    {
        Ipv4Address addr = remoteHostIpv4->GetAddress(i, 0).GetLocal();
        if (addr != Ipv4Address("127.0.0.1"))
        {
            remoteHostActualAddress = addr;
            break;
        }
    }
    NS_LOG_UNCOND("Remote host address: " << remoteHostActualAddress
                                          << " (Gateway: " << remoteHostIpv4Address << ")");

    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = nrEpcHelper->AssignUeIpv4Address(ueNetDev);
    SetUeDefaultRoutes(ueNodes, nrEpcHelper);
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);

    UeDistanceMap ueDistances;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Vector pos = ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        ueDistances[ueIpIface.GetAddress(i)] = static_cast<float>(CalculateDistance(gnbPos, pos));
    }
    const UeDistanceMap* traceDistances = o.traceUeDistance ? &ueDistances : nullptr;

    // O-RAN: the RIC is initialized before the applications, so that the VR bitrate logic
    // module has its data repository when the adaptation algorithms are created
    Ptr<OranLogicVrBitrate> oranLogicVrBitrate = CreateObject<OranLogicVrBitrate>();
    OranRicConfig ricConfig;
    ricConfig.verbose = true;
    ricConfig.lmQueryInterval = Seconds(5);
    ricConfig.lmQueryMaxWaitTime = Seconds(0.010);
    ricConfig.e2NodeInactivityInterval = Seconds(0.2);
    Ptr<OranHelper> oranHelper = CreateOranHelper(ricConfig);
    oranHelper->SetDefaultLogicModule(
        "ns3::OranLmNr2NrRsrpHandover",
        "ProcessingDelayRv",
        StringValue("ns3::NormalRandomVariable[Mean=0.005|Variance=0.000031]"));
    Ptr<OranNearRtRic> nearRtRic = oranHelper->CreateNearRtRic();
    oranLogicVrBitrate->SetDataRepository(nearRtRic->Data());

    const BurstGeneratorSetup generator =
        ConfigureBurstGenerator(o.vr.burstGeneratorType, oranLogicVrBitrate);
    const std::string protocol = generator.socketFactory;

    // The RIC host serves bitrate requests and/or terminates the E2 interface
    E2TransportStats e2Stats;
    e2Stats.printEachMessage = o.e2Trace;
    const bool ricBitrateOverNetwork = o.vr.ricNetwork && generator.usesRicServer;
    if (ricBitrateOverNetwork || o.vr.e2Network)
    {
        RicHost ricHost = CreateRicHost(nrEpcHelper->GetPgwNode(), remoteHost, internet);
        if (ricBitrateOverNetwork)
        {
            InstallRicBitrateServer(ricHost, oranLogicVrBitrate, o.simTime + Seconds(4));
        }
        if (o.vr.e2Network)
        {
            e2Stats.Connect(EnableE2NetworkTransport(nearRtRic, ricHost.node),
                            ricHost.node->GetId());
        }
    }

    // The VR traffic: server on the remote host, one client per UE
    const uint16_t port = 1234;
    const uint32_t fragmentSize = 1200; // bytes

    // The dedicated QoS flow of the VR traffic. The filter on the server port matches the
    // downlink (server is the remote source) and the uplink (server is the remote destination)
    NrQosFlow vrFlow(vrQci);
    Ptr<NrQosRule> vrRule = Create<NrQosRule>();
    NrQosRule::PacketFilter vrFilter;
    vrFilter.remotePortStart = port;
    vrFilter.remotePortEnd = port;
    vrRule->Add(vrFilter);

    ConfigureVrApplicationDefaults(o.vr, fragmentSize, o.simTime);
    ApplicationContainer serverApp =
        InstallVrServer(protocol, port, remoteHost, o.simTime + Seconds(3));
    ApplicationContainer clientApps =
        InstallVrClients(protocol, remoteHostActualAddress, port, ueNodes, ueIpIface);
    Ptr<UniformRandomVariable> randomStart =
        CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                          DoubleValue(2.5),
                                                          "Max",
                                                          DoubleValue(3));

    // Traces
    const std::string distanceColumn = o.traceUeDistance ? ",Distance" : "";
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> burstTrace = ascii.CreateFileStream("burstTrace.csv");
    *burstTrace->GetStream() << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,BurstSize"
                             << distanceColumn << std::endl;
    Ptr<OutputStreamWrapper> fragmentTrace = ascii.CreateFileStream("fragmentTrace.csv");
    *fragmentTrace->GetStream()
        << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize" << distanceColumn
        << std::endl;

    // Fragments sent by the server, to correlate the sent and the received fragments
    Ptr<OutputStreamWrapper> txFragmentTrace = ascii.CreateFileStream("txFragmentTrace.csv");
    *txFragmentTrace->GetStream()
        << "DstAddress,EventTime_ns,TxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize" << distanceColumn
        << std::endl;
    serverApp.Get(0)->TraceConnectWithoutContext(
        "FragmentRx",
        MakeBoundCallback(&FragmentRx, txFragmentTrace, traceDistances));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Time startTime = Seconds(randomStart->GetValue());
        NS_LOG_UNCOND("STA" << i << " will start at " << startTime.As(Time::S));
        Ptr<Application> app = clientApps.Get(i);
        app->SetStartTime(startTime);
        app->TraceConnectWithoutContext("BurstRx",
                                        MakeBoundCallback(&BurstRx, burstTrace, traceDistances));
        app->TraceConnectWithoutContext(
            "FragmentRx",
            MakeBoundCallback(&FragmentRx, fragmentTrace, traceDistances));
    }
    clientApps.Stop(o.simTime + Seconds(3));

    for (uint32_t i = 0; i < ueNetDev.GetN(); ++i)
    {
        nrHelper->ActivateDedicatedQosFlow(ueNetDev.Get(i), vrFlow, vrRule);
    }

    // O-RAN: E2 terminators of the UEs and of the gNBs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        DeployUeTerminator(ueNodes.Get(i), nearRtRic, Seconds(1), Seconds(1), Seconds(1));
    }

    oranHelper->SetE2NodeTerminator("ns3::OranE2NodeTerminatorNrGnb",
                                    "RegistrationIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=1]"),
                                    "SendIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    // 10 ms reporting period of the periodic triggers
    Config::SetDefault("ns3::OranReportTriggerPeriodic::IntervalRv",
                       StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    oranHelper->AddReporter("ns3::OranReporterLocation",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    AddUeStatsReporter(oranHelper, Seconds(0));

    std::cout << "Deploying ENB terminators and wiring reporters" << std::endl;
    OranE2NodeTerminatorContainer gnbTerminators =
        oranHelper->DeployTerminators(nearRtRic, gridScenario.GetBaseStations());
    if (generator.usesOranLm)
    {
        ConnectUeStatsReporters(gnbNetDev, gnbTerminators);
    }

    // The UE terminators activate themselves
    Simulator::Schedule(Seconds(1), &OranHelper::ActivateAndStartNearRtRic, oranHelper, nearRtRic);
    Simulator::Schedule(Seconds(1.5),
                        &OranHelper::ActivateE2NodeTerminators,
                        oranHelper,
                        gnbTerminators);

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueNodes);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(o.simTime + Seconds(4));
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
    if (o.sortFlowStats)
    {
        PrintSortedFlowStats(stats, classifier, outFile);
    }
    else
    {
        // The flows are measured from the 400 ms mark of the original cttc-nr-demo
        PrintFlowStats(stats, classifier, (o.simTime - MilliSeconds(400)).GetSeconds(), outFile);
    }
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

    if (o.vr.e2Network)
    {
        e2Stats.Print(std::cout);
    }
    Simulator::Destroy();
    return 0;
}

} // namespace vrnr
} // namespace ns3

#endif // VR_CTTC_NR_ORAN_SCENARIO_H
