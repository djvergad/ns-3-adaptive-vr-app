// SPDX-License-Identifier: GPL-2.0-only

#ifndef VR_NR_ORAN_COMMON_H
#define VR_NR_ORAN_COMMON_H

/**
 * @file vr-nr-oran-common.h
 * @brief Building blocks shared by the VR over NR with O-RAN examples.
 *
 * Used by vr-cttc-nr-oran.cc, vr-cttc-nr-oran-distance.cc, vr-nr-oran-handover.cc and
 * vr-nr-oran-handover-bg.cc. The functions are header-only so that the examples can also be
 * built as single-file scratch programs (scratch/ needs a link to this header as well).
 *
 * Each function wraps one setup step; the examples call them in the order they need. The
 * order matters for reproducibility: it decides node ids and random stream assignment.
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

/// UDP port of the RIC VR bitrate service (OranVrBitrateServer).
const uint16_t RIC_BITRATE_PORT = 9000;

/**
 * VR application and O-RAN options common to all the examples.
 *
 * The examples set their own defaults before calling AddCommandLineArgs().
 */
struct VrOranOptions
{
    std::string appRate{"20Mbps"};           ///< VR application target data rate
    double frameRate{60};                    ///< VR application frame rate [FPS]
    std::string vrAppName{"VirusPopper"};    ///< VR application name (VrBurstGenerator model)
    std::string burstGeneratorType{"model"}; ///< Adaptation algorithm, see ConfigureBurstGenerator
    bool e2Network{true};  ///< Carry the E2 interface (UEs/gNBs <-> RIC) over the network
    bool ricNetwork{true}; ///< Query the RIC bitrate service over the network

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
        cmd.AddValue("e2Network",
                     "Carry the E2 messages between the UEs/gNBs and the RIC over the network",
                     e2Network);
        cmd.AddValue("ricNetwork",
                     "Request the VR bitrate from the RIC xApp over the network",
                     ricNetwork);
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
 * Aborts on an unknown type. The TCP variant is left to the caller.
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
 * Set the defaults of the VR burst generator and of the bursty application server.
 *
 * @param opts The VR options.
 * @param fragmentSize The fragment size [bytes].
 * @param appDuration The BurstyApplicationServer application duration.
 */
inline void
ConfigureVrApplicationDefaults(const VrOranOptions& opts, uint32_t fragmentSize, Time appDuration)
{
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(appDuration));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(fragmentSize));
    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(opts.frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate",
                       DataRateValue(DataRate(opts.appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(opts.vrAppName));
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
 * @param internet The internet stack helper to install on the RIC host.
 * @return The RIC host.
 */
inline RicHost
CreateRicHost(Ptr<Node> pgw, Ptr<Node> remoteHost, InternetStackHelper& internet)
{
    NodeContainer ricHostContainer;
    ricHostContainer.Create(1);
    Ptr<Node> ricHost = ricHostContainer.Get(0);
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

/// Near-RT RIC configuration shared by the examples.
struct OranRicConfig
{
    bool verbose{false};                       ///< OranHelper verbosity
    Time lmQueryInterval{Seconds(5)};          ///< Interval of the logic module queries
    Time lmQueryMaxWaitTime{Seconds(0)};       ///< Max wait for the LMs, 0 waits for all
    Time e2NodeInactivityInterval{Seconds(2)}; ///< Interval of the E2 node inactivity checks
    std::string dbFile{"oran-repository.db"};  ///< SQLite data repository file
};

/**
 * Create an OranHelper with an SQLite data repository (the old database is deleted), the
 * no-op conflict mitigation module and the given query settings. The caller sets the
 * default logic module.
 *
 * @param config The RIC configuration.
 * @return The O-RAN helper.
 */
inline Ptr<OranHelper>
CreateOranHelper(const OranRicConfig& config)
{
    std::remove(config.dbFile.c_str());
    std::remove((config.dbFile + "-wal").c_str());
    std::remove((config.dbFile + "-shm").c_str());

    Ptr<OranHelper> oranHelper = CreateObject<OranHelper>();
    oranHelper->SetAttribute("Verbose", BooleanValue(config.verbose));
    oranHelper->SetAttribute("LmQueryInterval", TimeValue(config.lmQueryInterval));
    oranHelper->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(2)));
    oranHelper->SetAttribute("E2NodeInactivityIntervalRv",
                             StringValue(ConstantRv(config.e2NodeInactivityInterval)));
    oranHelper->SetAttribute("LmQueryMaxWaitTime", TimeValue(config.lmQueryMaxWaitTime));
    oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue("DROP"));
    oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite",
                                  "DatabaseFile",
                                  StringValue(config.dbFile));
    oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");
    return oranHelper;
}

/**
 * Create the E2 terminator of a UE with location, cell info and RSRP/RSRQ reporters, attach
 * it to the UE and schedule its activation.
 *
 * @param ue The UE node.
 * @param ric The Near-RT RIC.
 * @param registrationInterval The registration interval.
 * @param sendInterval The report send interval.
 * @param activationTime When the terminator is activated.
 * @return The UE terminator.
 */
inline Ptr<OranE2NodeTerminatorNrUe>
DeployUeTerminator(Ptr<Node> ue,
                   Ptr<OranNearRtRic> ric,
                   Time registrationInterval,
                   Time sendInterval,
                   Time activationTime)
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
                               StringValue(ConstantRv(registrationInterval)));
    ueTerminator->SetAttribute("SendIntervalRv", StringValue(ConstantRv(sendInterval)));
    ueTerminator->AddReporter(locationReporter);
    ueTerminator->AddReporter(cellInfoReporter);
    ueTerminator->AddReporter(rsrpReporter);
    ueTerminator->Attach(ue);

    Simulator::Schedule(activationTime, &OranE2NodeTerminatorNrUe::Activate, ueTerminator);
    return ueTerminator;
}

/**
 * Add a periodic OranReporterNrUeStats to the reporters of the gNB terminators deployed
 * next by the helper.
 *
 * @param oranHelper The O-RAN helper.
 * @param holDelayTrigger The HOL delay that triggers an immediate report (0 disables).
 */
inline void
AddUeStatsReporter(Ptr<OranHelper> oranHelper, Time holDelayTrigger)
{
    oranHelper->AddReporter("ns3::OranReporterNrUeStats",
                            "HolDelayTrigger",
                            TimeValue(holDelayTrigger),
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
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

/**
 * Install the VR server (BurstyApplicationServer) on the remote host.
 *
 * @param socketFactory The socket factory type id.
 * @param port The server port.
 * @param remoteHost The remote host.
 * @param stopTime The stop time of the server.
 * @return The server application.
 */
inline ApplicationContainer
InstallVrServer(const std::string& socketFactory,
                uint16_t port,
                Ptr<Node> remoteHost,
                Time stopTime)
{
    BurstyApplicationServerHelper server(socketFactory,
                                         InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = server.Install(remoteHost);
    serverApp.Start(Seconds(0));
    serverApp.Stop(stopTime);
    return serverApp;
}

/**
 * Install a VR client (BurstyApplicationClient) on each UE, bound to the UE address.
 * The caller sets the start and stop times.
 *
 * @param socketFactory The socket factory type id.
 * @param server The address of the VR server.
 * @param port The server port.
 * @param ues The UE nodes.
 * @param ueIfaces The UE IPv4 interfaces, in the same order as ues.
 * @return The client applications, in the same order as ues.
 */
inline ApplicationContainer
InstallVrClients(const std::string& socketFactory,
                 Ipv4Address server,
                 uint16_t port,
                 const NodeContainer& ues,
                 const Ipv4InterfaceContainer& ueIfaces)
{
    BurstyApplicationClientHelper client(socketFactory, InetSocketAddress(server, port));
    ApplicationContainer clientApps = client.Install(ues);
    for (uint32_t u = 0; u < clientApps.GetN(); ++u)
    {
        clientApps.Get(u)->SetAttribute("Local",
                                        AddressValue(InetSocketAddress(ueIfaces.GetAddress(u), 0)));
    }
    return clientApps;
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
        m_ricNodeId = ricNodeId;
        transport->TraceConnectWithoutContext("E2Tx", MakeCallback(&E2TransportStats::Tx, this));
        transport->TraceConnectWithoutContext("E2Rx", MakeCallback(&E2TransportStats::Rx, this));
    }

    /**
     * Print the statistics.
     *
     * @param os The output stream.
     */
    void Print(std::ostream& os) const
    {
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
};

} // namespace vrnr
} // namespace ns3

#endif // VR_NR_ORAN_COMMON_H
