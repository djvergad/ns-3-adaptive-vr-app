// SPDX-License-Identifier: GPL-2.0-only

#ifndef VR_NR_ORAN_HANDOVER_SCENARIO_H
#define VR_NR_ORAN_HANDOVER_SCENARIO_H

/**
 * @file vr-nr-oran-handover-scenario.h
 * @brief Downlink VR traffic to a moving UE that is handed over between NR gNBs by an O-RAN
 * xApp, optionally with one stationary UE next to each gNB.
 *
 * Scenario: a row of gNBs (default 3) and one UE that moves along the row at constant speed.
 * A remote (edge) host streams VR traffic (BurstyApplicationServer with the VrBurstGenerator)
 * to the UE through the core network. The VR application, transport and O-RAN settings are
 * the common ones of vr-nr-oran-common.h. The Near-RT RIC runs OranLmNr2NrRsrpHandover, which hands
 * the UE over to the strongest gNB. With the default parameters the UE crosses two cell borders, so
 * two handovers happen. With --handover=false the UE stays attached to the first gNB and, as it
 * moves away, the received VR throughput degrades and finally drops to zero.
 *
 * With --backgroundUes one stationary UE is placed next to every gNB and receives the same VR
 * traffic. This shows the adaptation algorithm (e.g. --burstGeneratorType=oran-util-udp)
 * sharing the capacity of each cell between several UEs, and the moving UE changing the load
 * of the cells it is handed over to.
 *
 * Every UE starts its stream at 1 s and receives --simTime of VR traffic. The simulation ends
 * 1 s after the streams, so that the last frames can arrive.
 *
 * Programs: vr-nr-oran-handover.cc (single UE) and vr-nr-oran-handover-bg.cc (background
 * UEs). Every default can be overridden from the command line; see --PrintHelp.
 *
 * Output (<prefix> is set with --outPrefix):
 *  - Each handover is printed on the terminal as it happens, plus a per-interval summary line.
 *  - <prefix>-timeseries.csv: per-UE, per-interval position, serving cell, received VR
 *    throughput, mean delay and packet loss. UE 0 is the moving one ("moving"), the others
 *    are "static".
 *  - <prefix>-gnb-positions.csv: gNB locations.
 *  - <prefix>-gnb-load.csv: per-gNB downlink throughput and RSRP of the moving UE.
 *  - <prefix>-gnb-dl.csv: one line per downlink packet reaching a gNB from the core network.
 * plot-vr-handover.py and plot-vr-handover-bg.py plot the results of a run with and without
 * --handover.
 */

#include "vr-nr-oran-common.h"

#include "ns3/mobility-module.h"

#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace ns3
{
namespace vrnr
{

/// Options of the handover scenario. The programs set the defaults.
struct HandoverOptions
{
    // Scenario
    bool handover{true};       ///< Enable the O-RAN handover xApp
    uint16_t numGnbs{3};       ///< gNBs placed in a row
    double gnbSpacing{200};    ///< Distance between neighbouring gNBs [m]
    double speed{20};          ///< Speed of the moving UE [m/s]
    double gnbHeight{10};      ///< gNB antenna height [m]
    double ueOffsetY{20};      ///< Lateral distance of the UE path from the gNB row [m]
    bool backgroundUes{true};  ///< One stationary UE with VR traffic next to each gNB
    double bgOffsetX{10};      ///< x offset of the stationary UEs from their gNB [m]
    double bgOffsetY{10};      ///< y offset of the stationary UEs from their gNB [m]
    Time simTime{Seconds(22)}; ///< Duration of each UE's VR stream

    // Radio
    double centralFrequency{3.5e9}; ///< Central frequency [Hz]
    double bandwidth{40e6};         ///< Bandwidth [Hz]
    /// LOS/NLOS state of every gNB-UE pair; "Default" draws it randomly per RngRun, which can
    /// make a distant gNB the strongest one and send the moving UE to the wrong cell
    std::string channelCondition{"LOS"};
    uint16_t numerology{1}; ///< NR numerology
    double txPower{20};     ///< gNB total TX power [dBm]

    // VR application, transport and O-RAN
    VrOranOptions vr;                      ///< Common options
    double hysteresisDb{2.0};              ///< Handover RSRP hysteresis [dB]
    Time timeToTrigger{MilliSeconds(200)}; ///< Handover time-to-trigger

    // Output
    std::string outPrefix{"vr-handover-bg"}; ///< Prefix of the output files
    Time reportInterval{Seconds(1)};         ///< Interval of the summary output
};

namespace handover
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

/// State shared by the trace sinks of the scenario.
struct State
{
    std::vector<UeInterval> ueStats;        ///< Current interval statistics, per UE
    std::vector<uint64_t> gnbDlBytes;       ///< Downlink bytes reaching each gNB in the interval
    std::map<uint32_t, uint32_t> nodeToGnb; ///< Node id -> gNB index
    std::map<uint32_t, uint32_t> ipToUe;    ///< UE IPv4 address -> UE index
    std::vector<uint16_t> cellIds;          ///< Cell id of each gNB
    std::vector<double> rsrp;               ///< Last RSRP [dBm] of the moving UE per gNB index
    Ptr<OutputStreamWrapper> gnbDlTrace;    ///< Per-packet gNB downlink trace
    std::vector<Ptr<NrUeNetDevice>> ueDevs; ///< The UE devices
    std::vector<Ptr<MobilityModel>> ueMob;  ///< The UE mobility models
    std::vector<double> ueTotalRxBytes;     ///< Total VR bytes received by each UE
    Time interval;                          ///< Reporting interval
    uint32_t handoverCount{0};              ///< Handovers completed so far
    Time stopReporting;                     ///< No reports are produced after this time
    std::vector<Row> rows;                  ///< Per-UE time series
    std::vector<GnbRow> gnbRows;            ///< Per-gNB time series
    std::map<uint64_t, uint32_t> sentFragments; ///< Fragment key -> interval in which it was sent
    std::set<uint64_t> rxFragments;             ///< Keys of the fragments received by the UEs
};

/// @return The state of the scenario.
inline State&
GetState()
{
    static State state;
    return state;
}

/**
 * @param ue The UE index.
 * @param h The fragment header.
 * @return A key identifying a VR fragment of a given UE (burst and fragment sequence).
 */
inline uint64_t
FragmentKey(uint32_t ue, const SeqTsSizeFragHeader& h)
{
    return (static_cast<uint64_t>(ue) << 56) |
           ((static_cast<uint64_t>(h.GetSeq()) & 0xFFFFFF) << 32) | h.GetFragSeq();
}

/**
 * Called when the VR server sends an IP packet towards the core network.
 *
 * @param packet The packet, with its PPP header.
 */
inline void
ServerTx(Ptr<const Packet> packet)
{
    State& s = GetState();
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
    auto it = s.ipToUe.find(ip.GetDestination().Get());
    if (it == s.ipToUe.end())
    {
        return;
    }
    p->RemoveHeader(ip);
    p->RemoveHeader(udp);
    p->PeekHeader(frag);
    s.sentFragments.emplace(
        FragmentKey(it->second, frag),
        static_cast<uint32_t>(Simulator::Now().GetSeconds() / s.interval.GetSeconds()));
}

/**
 * @param cellId A cell id.
 * @return The gNB index of the given cell id, or -1.
 */
inline int
GnbIndexOfCell(uint16_t cellId)
{
    const State& s = GetState();
    for (size_t i = 0; i < s.cellIds.size(); ++i)
    {
        if (s.cellIds[i] == cellId)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/**
 * @param context A Config path context ("/NodeList/<id>/...").
 * @return The node id in the context.
 */
inline uint32_t
NodeIdFromContext(const std::string& context)
{
    size_t pos = context.find("/NodeList/") + 10;
    return std::stoul(context.substr(pos, context.find('/', pos) - pos));
}

/**
 * RxFromS1u trace sink: a downlink packet reached a gNB from the core network.
 *
 * @param context The trace context.
 * @param packet The packet.
 */
inline void
GnbRxFromS1u(std::string context, Ptr<Packet> packet)
{
    State& s = GetState();
    auto it = s.nodeToGnb.find(NodeIdFromContext(context));
    if (it == s.nodeToGnb.end())
    {
        return;
    }
    s.gnbDlBytes[it->second] += packet->GetSize();
    *s.gnbDlTrace->GetStream() << Simulator::Now().GetSeconds() << "," << s.cellIds[it->second]
                               << "," << packet->GetSize() << "\n";
}

/**
 * FragmentRx trace sink of the VR clients.
 *
 * @param ue The UE index.
 * @param fragment The fragment.
 * @param from The source address.
 * @param to The destination address.
 * @param header The fragment header.
 */
inline void
FragmentRx(uint32_t ue,
           Ptr<const Packet> fragment,
           const Address& from,
           const Address& to,
           const SeqTsSizeFragHeader& header)
{
    State& s = GetState();
    s.rxFragments.insert(FragmentKey(ue, header));
    s.ueStats[ue].rxBytes += fragment->GetSize();
    s.ueTotalRxBytes[ue] += fragment->GetSize();
    s.ueStats[ue].delaySum += (Simulator::Now() - header.GetTs()).GetSeconds();
    s.ueStats[ue].delayCount++;
}

/**
 * RSRP measurements of the moving UE.
 *
 * @param rnti The RNTI.
 * @param cellId The measured cell.
 * @param rsrp The RSRP [dBm].
 * @param rsrq The RSRQ [dB].
 * @param isServingCell Whether the cell is the serving one.
 * @param componentCarrierId The component carrier.
 */
inline void
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
        GetState().rsrp[idx] = rsrp;
    }
}

/**
 * HandoverStart trace sink.
 *
 * @param imsi The UE IMSI.
 * @param cellId The source cell.
 * @param rnti The RNTI.
 * @param targetCellId The target cell.
 */
inline void
HandoverStart(uint64_t imsi, uint16_t cellId, uint16_t rnti, uint16_t targetCellId)
{
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] HANDOVER START:    UE IMSI " << imsi << " cell " << cellId << " -> cell "
              << targetCellId << std::endl;
}

/**
 * HandoverEndOk trace sink.
 *
 * @param imsi The UE IMSI.
 * @param cellId The new cell.
 * @param rnti The new RNTI.
 */
inline void
HandoverEndOk(uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    State& s = GetState();
    s.handoverCount++;
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] HANDOVER COMPLETE: UE IMSI " << imsi << " is now in cell " << cellId
              << " (RNTI " << rnti << ") - handover #" << s.handoverCount << std::endl;
}

/**
 * RadioLinkFailure trace sink.
 *
 * @param imsi The UE IMSI.
 * @param cellId The cell.
 * @param rnti The RNTI.
 */
inline void
RadioLinkFailure(uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::cout << std::fixed << std::setprecision(3) << "[" << Simulator::Now().GetSeconds()
              << " s] RADIO LINK FAILURE: UE IMSI " << imsi << " in cell " << cellId << std::endl;
}

/// Record the statistics of the interval that just ended and schedule the next report.
inline void
Report()
{
    State& s = GetState();
    const double t = Simulator::Now().GetSeconds();
    const double dt = s.interval.GetSeconds();

    std::ostringstream line;
    line << std::fixed << std::setprecision(1) << t << ",x=" << s.ueMob[0]->GetPosition().x;
    for (uint32_t u = 0; u < s.ueDevs.size(); ++u)
    {
        Row row;
        row.t = t;
        row.ue = u;
        row.uePos = s.ueMob[u]->GetPosition();
        row.cell = s.ueDevs[u]->GetRrc()->GetCellId();
        row.rxMbps = s.ueStats[u].rxBytes * 8 / dt / 1e6;
        row.delayMs =
            s.ueStats[u].delayCount ? s.ueStats[u].delaySum / s.ueStats[u].delayCount * 1000 : 0.0;
        s.rows.push_back(row);
        line << " | UE" << u << " cell" << row.cell << " " << row.rxMbps << "Mbps " << row.delayMs
             << "ms";
        s.ueStats[u] = UeInterval();
    }
    GnbRow gr;
    gr.t = t;
    for (uint64_t bytes : s.gnbDlBytes)
    {
        gr.gnbMbps.push_back(bytes * 8 / dt / 1e6);
    }
    gr.rsrp = s.rsrp;
    s.gnbRows.push_back(gr);
    std::cout << line.str() << std::endl;

    s.gnbDlBytes.assign(s.cellIds.size(), 0);
    if (Simulator::Now() + s.interval <= s.stopReporting)
    {
        Simulator::Schedule(s.interval, &Report);
    }
}

/**
 * Write the per-UE time series, with the loss per UE and interval: fragments sent in the
 * interval and never received by the end of the simulation.
 *
 * @param os The output stream.
 * @param numUes The number of UEs.
 */
inline void
WriteTimeSeries(std::ostream& os, uint32_t numUes)
{
    const State& s = GetState();
    const size_t numIntervals = s.gnbRows.size();
    std::vector<std::vector<uint64_t>> sent(numUes, std::vector<uint64_t>(numIntervals + 1, 0));
    std::vector<std::vector<uint64_t>> lost = sent;
    for (const auto& [key, interval] : s.sentFragments)
    {
        const uint32_t ue = key >> 56;
        if (interval < numIntervals)
        {
            sent[ue][interval]++;
            lost[ue][interval] += s.rxFragments.count(key) ? 0 : 1;
        }
    }
    os << "Time_s,UeId,UeType,UeX_m,UeY_m,UeZ_m,ServingCell,RxMbps,MeanDelay_ms,SentPackets,"
          "LostPackets,LossPercent\n";
    for (size_t i = 0; i < s.rows.size(); ++i)
    {
        const Row& r = s.rows[i];
        // The rows of interval k are rows[k * numUes ... ], covering [t - interval, t).
        const size_t k = i / numUes;
        os << r.t << "," << r.ue << "," << (r.ue == 0 ? "moving" : "static") << "," << r.uePos.x
           << "," << r.uePos.y << "," << r.uePos.z << "," << r.cell << "," << r.rxMbps << ","
           << r.delayMs << "," << sent[r.ue][k] << "," << lost[r.ue][k] << ","
           << (sent[r.ue][k] ? 100.0 * lost[r.ue][k] / sent[r.ue][k] : 0.0) << "\n";
    }
    os.flush();
}

/**
 * Write the per-gNB load time series.
 *
 * @param os The output stream.
 */
inline void
WriteGnbLoad(std::ostream& os)
{
    const State& s = GetState();
    os << "Time_s";
    for (uint16_t cell : s.cellIds)
    {
        os << ",gNB_cell" << cell << "_DlMbps";
    }
    for (uint16_t cell : s.cellIds)
    {
        os << ",MovingUeRsrp_cell" << cell << "_dBm";
    }
    os << "\n";
    for (const GnbRow& r : s.gnbRows)
    {
        os << r.t;
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
}

} // namespace handover

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
RunVrNrOranHandover(int argc, char* argv[], const std::string& program, HandoverOptions o)
{
    using namespace handover;
    State& s = GetState();

    CommandLine cmd(program);
    cmd.AddValue("handover", "Enable the O-RAN handover logic module", o.handover);
    cmd.AddValue("numGnbs", "Number of gNBs in the row", o.numGnbs);
    cmd.AddValue("gnbSpacing", "Distance between neighbouring gNBs [m]", o.gnbSpacing);
    cmd.AddValue("speed", "UE speed [m/s]", o.speed);
    cmd.AddValue("ueOffsetY", "Lateral distance of the UE path from the gNB row [m]", o.ueOffsetY);
    cmd.AddValue("outPrefix", "Prefix of the output files", o.outPrefix);
    cmd.AddValue("backgroundUes",
                 "Add one stationary UE with VR traffic next to each gNB",
                 o.backgroundUes);
    cmd.AddValue("bgOffsetX", "x offset of the stationary UEs from their gNB [m]", o.bgOffsetX);
    cmd.AddValue("bgOffsetY", "y offset of the stationary UEs from their gNB [m]", o.bgOffsetY);
    cmd.AddValue("simTime", "Duration of the VR stream of each UE", o.simTime);
    cmd.AddValue("frequency", "Central frequency [Hz]", o.centralFrequency);
    cmd.AddValue("bandwidth", "Bandwidth [Hz]", o.bandwidth);
    cmd.AddValue("numerology", "NR numerology", o.numerology);
    cmd.AddValue("txPower", "gNB total TX power [dBm]", o.txPower);
    cmd.AddValue("channelCondition",
                 "Channel condition {\"LOS\", \"NLOS\", \"Default\" (random 3GPP UMi)}",
                 o.channelCondition);
    o.vr.AddCommandLineArgs(cmd);
    cmd.AddValue("hysteresisDb", "Handover RSRP hysteresis [dB]", o.hysteresisDb);
    cmd.AddValue("timeToTrigger", "Handover time-to-trigger", o.timeToTrigger);
    cmd.AddValue("reportInterval", "Interval of the summary output", o.reportInterval);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(o.numGnbs < 2, "At least two gNBs are needed");

    // Every stream starts at streamStart and lasts simTime
    const Time streamStart = ORAN_START;
    const Time streamEnd = streamStart + o.simTime;
    const Time stopTime = streamEnd + Seconds(1); // drain the last frames
    s.interval = o.reportInterval;
    s.stopReporting = streamEnd;

    ConfigureCommonDefaults(o.vr, o.simTime);
    LogComponentEnable("OranNearRtRic", (LogLevel)(LOG_PREFIX_TIME | LOG_WARN));

    NrHelpers nr = CreateNrHelpers(o.vr);
    RemoteHost remoteHost = CreateRemoteHost(nr.epc, o.vr);

    // Nodes and mobility: gNBs in a row, UE 0 moves along it, UE i > 0 sits at gNB i - 1
    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(o.numGnbs);
    const uint32_t numUes = 1 + (o.backgroundUes ? o.numGnbs : 0);
    ueNodes.Create(numUes);

    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < o.numGnbs; ++i)
    {
        gnbPos->Add(Vector(o.gnbSpacing * i, 0, o.gnbHeight));
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
    ueMobility->SetPosition(Vector(0, o.ueOffsetY, 1.5));
    ueMobility->SetVelocity(Vector(o.speed, 0, 0));
    for (uint32_t u = 1; u < numUes; ++u)
    {
        Ptr<ConstantVelocityMobilityModel> bgMob =
            ueNodes.Get(u)->GetObject<ConstantVelocityMobilityModel>();
        bgMob->SetPosition(Vector(o.gnbSpacing * (u - 1) + o.bgOffsetX, o.bgOffsetY, 1.5));
        bgMob->SetVelocity(Vector(0, 0, 0));
    }

    // NR spectrum
    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(o.centralFrequency, o.bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", o.channelCondition, "ThreeGpp");
    if (o.channelCondition == "Default")
    {
        channelHelper->SetChannelConditionModelAttribute("UpdatePeriod",
                                                         TimeValue(MilliSeconds(0)));
    }
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    NetDeviceContainer gnbDevs = nr.nr->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nr.nr->InstallUeDevice(ueNodes, allBwps);
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        NrHelper::GetGnbPhy(gnbDevs.Get(i), 0)
            ->SetAttribute("Numerology", UintegerValue(o.numerology));
        NrHelper::GetGnbPhy(gnbDevs.Get(i), 0)->SetAttribute("TxPower", DoubleValue(o.txPower));
    }
    SetUeTxPower(ueDevs, o.vr.ueTxPower);

    // IP configuration of the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = nr.epc->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    SetUeDefaultRoutes(ueNodes, nr.epc);

    // X2 between all gNBs (needed for X2 handover); initial attachment of the moving UE to
    // the first gNB and of each stationary UE to its gNB
    nr.nr->AddX2Interface(gnbNodes);
    nr.nr->AttachToGnb(ueDevs.Get(0), gnbDevs.Get(0));
    for (uint32_t u = 1; u < numUes; ++u)
    {
        nr.nr->AttachToGnb(ueDevs.Get(u), gnbDevs.Get(u - 1));
    }
    ActivateVrQosFlow(nr.nr, ueDevs, o.vr);

    for (uint32_t u = 0; u < numUes; ++u)
    {
        s.ueDevs.push_back(ueDevs.Get(u)->GetObject<NrUeNetDevice>());
        s.ueMob.push_back(ueNodes.Get(u)->GetObject<MobilityModel>());
        s.ipToUe[ueIpIface.GetAddress(u).Get()] = u;
    }
    s.ueStats.assign(numUes, UeInterval());
    s.ueTotalRxBytes.assign(numUes, 0);
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        s.nodeToGnb[gnbNodes.Get(i)->GetId()] = i;
        s.cellIds.push_back(gnbDevs.Get(i)->GetObject<NrGnbNetDevice>()->GetCellId());
    }
    s.gnbDlBytes.assign(o.numGnbs, 0);
    s.rsrp.assign(o.numGnbs, std::nan(""));

    // O-RAN: Near-RT RIC with the NR RSRP handover logic module (or a no-op one)
    Ptr<OranLogicVrBitrate> vrBitrateLm = CreateObject<OranLogicVrBitrate>();
    const BurstGeneratorSetup generator =
        ConfigureBurstGenerator(o.vr.burstGeneratorType, vrBitrateLm);
    Ptr<OranHelper> oranHelper = CreateOranHelper(o.vr);
    if (o.handover)
    {
        oranHelper->SetDefaultLogicModule(
            "ns3::OranLmNr2NrRsrpHandover",
            "HysteresisDb",
            DoubleValue(o.hysteresisDb),
            "TimeToTrigger",
            TimeValue(o.timeToTrigger),
            "ProcessingDelayRv",
            StringValue("ns3::ConstantRandomVariable[Constant=0.005]"));
    }
    else
    {
        oranHelper->SetDefaultLogicModule("ns3::OranLmNoop");
    }
    OranDeployment oran;
    DeployOran(oranHelper,
               o.vr,
               generator,
               vrBitrateLm,
               nr.epc->GetPgwNode(),
               remoteHost.node,
               gnbNodes,
               gnbDevs,
               ueNodes,
               stopTime,
               oran);
    // RSRP of the moving UE towards each gNB, for the gNB load trace
    s.ueDevs[0]->GetPhy(0)->TraceConnectWithoutContext("ReportUeMeasurements",
                                                       MakeCallback(&UeMeasurements));

    // Downlink VR traffic: server on the remote host, one client per UE
    ApplicationContainer serverApp =
        InstallVrServer(generator.socketFactory, remoteHost.node, stopTime);
    ApplicationContainer clientApps =
        InstallVrClients(generator.socketFactory, remoteHost.address, ueNodes, ueIpIface, stopTime);
    for (uint32_t u = 0; u < numUes; ++u)
    {
        Ptr<Application> client = clientApps.Get(u);
        client->SetStartTime(streamStart);
        client->TraceConnectWithoutContext("FragmentRx", MakeBoundCallback(&FragmentRx, u));
    }

    // Traces
    AsciiTraceHelper ascii;
    s.gnbDlTrace = ascii.CreateFileStream(o.outPrefix + "-gnb-dl.csv");
    *s.gnbDlTrace->GetStream() << "Time_s,CellId,Bytes\n";
    Ptr<OutputStreamWrapper> summaryTrace = ascii.CreateFileStream(o.outPrefix + "-timeseries.csv");
    Ptr<OutputStreamWrapper> gnbLoadTrace = ascii.CreateFileStream(o.outPrefix + "-gnb-load.csv");
    Ptr<OutputStreamWrapper> gnbPosTrace =
        ascii.CreateFileStream(o.outPrefix + "-gnb-positions.csv");
    *gnbPosTrace->GetStream() << "CellId,X_m,Y_m,Z_m\n";
    for (uint32_t i = 0; i < gnbNodes.GetN(); ++i)
    {
        Vector p = gnbNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        *gnbPosTrace->GetStream() << s.cellIds[i] << "," << p.x << "," << p.y << "," << p.z << "\n";
    }
    remoteHost.device->TraceConnectWithoutContext("MacTx", MakeCallback(&ServerTx));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NrEpcGnbApplication/RxFromS1u",
                    MakeCallback(&GnbRxFromS1u));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverStart",
                                  MakeCallback(&HandoverStart));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrGnbRrc/HandoverEndOk",
                                  MakeCallback(&HandoverEndOk));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/NrUeRrc/RadioLinkFailure",
                                  MakeCallback(&RadioLinkFailure));
    Simulator::Schedule(s.interval, &Report);

    std::cout << "Handover " << (o.handover ? "ENABLED" : "DISABLED") << ", " << o.numGnbs
              << " gNBs and " << numUes << " UEs, spacing " << o.gnbSpacing << " m, UE speed "
              << o.speed << " m/s, VR rate " << o.vr.appRate << ", burst generator "
              << o.vr.burstGeneratorType << std::endl;

    Simulator::Stop(stopTime);
    Simulator::Run();

    WriteTimeSeries(*summaryTrace->GetStream(), numUes);
    WriteGnbLoad(*gnbLoadTrace->GetStream());

    std::cout << "Handovers completed: " << s.handoverCount << std::endl;
    for (uint32_t u = 0; u < numUes; ++u)
    {
        std::cout << "UE " << u << (u == 0 ? " (moving)" : " (static)") << ": received "
                  << s.ueTotalRxBytes[u] * 8 / 1e6 << " Mbit" << std::endl;
    }
    oran.e2Stats.Print(std::cout);
    Simulator::Destroy();
    return 0;
}

} // namespace vrnr
} // namespace ns3

#endif // VR_NR_ORAN_HANDOVER_SCENARIO_H
