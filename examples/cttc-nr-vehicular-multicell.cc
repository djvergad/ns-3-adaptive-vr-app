// Copyright (c) 2025
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @ingroup examples
 * @file cttc-nr-vehicular-multicell.cc
 * @brief NR multi-cell vehicular scenario with constant-velocity mobility and handover
 *
 * This example sets up an NR network with multiple gNBs along a line and a set of
 * vehicular UEs moving at constant velocity across cells. It uses the 3GPP TR 38.901
 * channel model and configures realistic antenna arrays and bearers. Downlink UDP
 * traffic is generated from a remote host to each UE. FlowMonitor reports throughput
 * and delay.
 *
 * Example usage:
 * ./ns3 run "cttc-nr-vehicular-multicell --gNbNum=3 --ueNum=12 --speed=25 --cellDist=500
 * --simTime=20s"
 */

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/oran-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CttcNrVehicularMultiCell");

int
main(int argc, char* argv[])
{
    LogComponentEnable("OranLogicVrBitrate", LOG_LEVEL_ALL);

    // Scenario parameters
    uint16_t ueNum = 3;       // reduced for faster execution
    double isd = 200.0;       // inter-site distance (m) - reduced
    double bsHeight = 25.0;   // gNB height in meters
    double ueHeight = 1.5;    // UE height in meters
    double speedMps = 20.0;   // vehicular speed (m/s)
    uint32_t lanes = 2;       // number of parallel lanes (for visualization spacing)
    double laneSpacing = 5.0; // distance between lanes (m)
    uint8_t numRings = 0;     // 0 -> 1 site (3 gNBs if TRIPLE, 1 if SINGLE)

    // Simulation times
    Time simTime = MilliSeconds(5000);        // increased from 500ms to 5000ms
    Time udpAppStartTime = MilliSeconds(1000); // delayed to allow ORAN infrastructure initialization

    // Traffic parameters
    uint32_t udpPacketSize = 512;
    uint32_t lambda = 100;                 // packets per second (each UE) - very low for fast sim
    std::string appRate = "50Mbps";        // VR target data rate
    double frameRate = 60;                 // VR frame rate (FPS)
    std::string vrAppName = "VirusPopper"; // VR app name
    std::string burstGeneratorType = "model"; // {model}

    // Spectrum parameters
    uint16_t numerology = 2;         // 60 kHz SCS
    double centralFrequency = 3.5e9; // FR1 typical vehicular freq
    double bandwidth = 40e6;         // 40 MHz
    double totalTxPowerDbm = 46.0;   // gNB Tx power

    // Output
    std::string simTag = "vehicular";
    std::string outputDir = "./";

    CommandLine cmd(__FILE__);
    cmd.AddValue("ueNum", "Total number of vehicular UEs", ueNum);
    cmd.AddValue("isd", "Inter-site distance (m)", isd);
    cmd.AddValue("speed", "Vehicle speed (m/s)", speedMps);
    cmd.AddValue("lanes", "Number of lanes", lanes);
    cmd.AddValue("laneSpacing", "Lane spacing (m)", laneSpacing);
    cmd.AddValue("numRings", "Hex grid outer rings (0-5)", numRings);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("udpPacketSize", "UDP packet size (bytes)", udpPacketSize);
    cmd.AddValue("lambda", "Packets per second per UE", lambda);
    cmd.AddValue("appRate", "VR target data rate", appRate);
    cmd.AddValue("frameRate", "VR frame rate [FPS]", frameRate);
    cmd.AddValue("vrAppName", "VR application name", vrAppName);
    cmd.AddValue("burstGeneratorType", "Burst generator type {model}", burstGeneratorType);
    cmd.AddValue("centralFrequency", "Carrier frequency (Hz)", centralFrequency);
    cmd.AddValue("bandwidth", "Channel bandwidth (Hz)", bandwidth);
    cmd.AddValue("numerology", "SCS numerology index", numerology);
    cmd.AddValue("totalTxPowerDbm", "gNB total Tx Power (dBm)", totalTxPowerDbm);
    cmd.AddValue("simTag", "Tag appended to outputs", simTag);
    cmd.AddValue("outputDir", "Directory for outputs", outputDir);
    cmd.Parse(argc, argv);

    NS_ABORT_IF(centralFrequency < 0.5e9 || centralFrequency > 7.2e9); // FR1 range guard

    // Scenario layout: simple linear multi-site with vehicular UE mobility
    // Use GridScenarioHelper for simplicity (avoids gnuplot hang from HexagonalGrid)
    GridScenarioHelper grid;
    uint16_t gNbNum = (numRings == 0) ? 1 : (numRings == 1 ? 3 : 5); // simple mapping
    grid.SetRows(1);
    grid.SetColumns(gNbNum);
    grid.SetHorizontalBsDistance(isd);
    grid.SetVerticalBsDistance(10.0);
    grid.SetBsHeight(bsHeight);
    grid.SetUtHeight(ueHeight);
    grid.SetSectorization(GridScenarioHelper::SINGLE);
    grid.SetBsNumber(gNbNum);
    grid.SetUtNumber(ueNum);
    grid.SetScenarioHeight(isd / 2);      // UEs distributed in area
    grid.SetScenarioLength(gNbNum * isd); // extend along X to cover sites
    int64_t rs = 1;
    rs += grid.AssignStreams(rs);
    grid.CreateScenario();

    NodeContainer ueContainer = grid.GetUserTerminals();
    NodeContainer gnbContainer = grid.GetBaseStations();

    // Now apply mobility manually to UEs
    for (uint32_t i = 0; i < ueContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueContainer.Get(i);
        Ptr<MobilityModel> mob = ue->GetObject<MobilityModel>();
        NS_ASSERT(mob);
        // Replace ConstantPosition with ConstantVelocity
        ue->AggregateObject(CreateObject<ConstantVelocityMobilityModel>());
        Ptr<ConstantVelocityMobilityModel> cvMob = ue->GetObject<ConstantVelocityMobilityModel>();
        cvMob->SetPosition(mob->GetPosition()); // keep grid position
        cvMob->SetVelocity(Vector(speedMps, 0.0, 0.0));
    }

    NS_LOG_INFO("Vehicular UEs: " << ueContainer.GetN() << ", gNBs: " << gnbContainer.GetN());

    // NR helpers
    Ptr<NrPointToPointEpcHelper> epc = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> bfHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nr = CreateObject<NrHelper>();
    nr->SetBeamformingHelper(bfHelper);
    nr->SetEpcHelper(epc);

    // Spectrum/channel
    BandwidthPartInfoPtrVector bwps;
    CcBwpCreator ccCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1);
    OperationBandInfo band = ccCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> ch = CreateObject<NrChannelHelper>();
    // Use UMa for city vehicular (can switch to RMa depending on scenario)
    ch->ConfigureFactories("UMa", "Default", "ThreeGpp");
    ch->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    ch->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));
    ch->AssignChannelsToBands({band});
    bwps = CcBwpCreator::GetAllBwps({band});

    Packet::EnableChecking();
    Packet::EnablePrinting();

    // Antennas
    bfHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nr->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nr->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nr->SetUeAntennaAttribute("AntennaElement",
                              PointerValue(CreateObject<IsotropicAntennaModel>()));
    nr->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nr->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nr->SetGnbAntennaAttribute("AntennaElement",
                               PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Install devices
    NetDeviceContainer gnbDevs = nr->InstallGnbDevice(gnbContainer, bwps);
    NetDeviceContainer ueDevs = nr->InstallUeDevice(ueContainer, bwps);

    rs += nr->AssignStreams(gnbDevs, rs);
    rs += nr->AssignStreams(ueDevs, rs);

    // Per-node PHY config
    double totalBw = bandwidth;
    double linPower = std::pow(10.0, totalTxPowerDbm / 10.0);
    NrHelper::GetGnbPhy(gnbDevs.Get(0), 0)->SetAttribute("Numerology", UintegerValue(numerology));
    NrHelper::GetGnbPhy(gnbDevs.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10.0 * std::log10((bandwidth / totalBw) * linPower)));

    // Internet and addressing
    auto [remoteHost, remoteAddr] = epc->SetupRemoteHost("100Gb/s", 2500, Seconds(0.000));

    // Get the actual P2P network interface address of the remote host
    // (not the gateway address, but the actual IP on the P2P link)
    Ipv4Address remoteHostActualAddress = remoteAddr;
    Ptr<Ipv4> remoteHostIpv4 = remoteHost->GetObject<Ipv4>();
    if (remoteHostIpv4)
    {
        // Get the first non-loopback interface's IP
        for (uint32_t i = 1; i < remoteHostIpv4->GetNInterfaces(); ++i)
        {
            Ipv4InterfaceAddress ifAddr = remoteHostIpv4->GetAddress(i, 0);
            Ipv4Address addr = ifAddr.GetLocal();
            if (addr != Ipv4Address("127.0.0.1"))
            {
                remoteHostActualAddress = addr;
                break;
            }
        }
    }
    NS_LOG_UNCOND("Remote host P2P address: " << remoteHostActualAddress);

    InternetStackHelper internet;
    internet.Install(ueContainer);
    internet.Install(gnbContainer); // gNBs need internet stack for backhaul routing
    Ipv4InterfaceContainer ueIpIface = epc->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // CRITICAL FIX: Set default gateway for UEs so their packets can reach the remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueContainer.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueContainer.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epc->GetUeDefaultGatewayAddress(), 1);
    }

    // CRITICAL FIX: Set routing on remote host so it knows how to reach UEs through PGW
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                               Ipv4Mask("255.0.0.0"),
                                               remoteAddr,
                                               1);

    // Initial attach to closest gNB; handovers occur under mobility
    nr->AttachToClosestGnb(ueDevs, gnbDevs);
    NS_LOG_UNCOND("UEs attached to gNBs");
    // Optional: ASCII mobility trace for debugging
    // MobilityHelper().EnableAsciiAll(Create<OutputStreamWrapper>(std::cout));

    // Adaptive VR downlink traffic (BurstyApplication with VrBurstGenerator)
    uint16_t vrPort = 5000;
    uint32_t fragmentSize = 1472; // bytes

    // Configure generator defaults
    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate", DataRateValue(DataRate(appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(vrAppName));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(fragmentSize));
    Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm", StringValue(""));
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(simTime));

    NS_LOG_UNCOND("Remote host IP: " << remoteAddr << ", VR server listening on port " << vrPort);

    // Create ORAN VR bitrate logic module (required for oran-util-udp)
    Ptr<ns3::OranLogicVrBitrate> oranLogicVrBitrate = CreateObject<ns3::OranLogicVrBitrate>();

    if (burstGeneratorType == "oran-util-udp")
    {
        NS_LOG_UNCOND("ORAN UDP adaptation algorithm selected with full infrastructure");
        // Configure ORAN logic module for adaptation
        Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::OranLogicVrBitrate",
                           PointerValue(Ptr<OranLogicVrBitrate>(oranLogicVrBitrate)));
        
        // Configure LCID (Logical Channel ID) for VR traffic bearer
        // Default is 5, but adjust if your bearer gets a different LCID
        // Typical range: 4-10 for dedicated bearers in NR/LTE
        Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::Lcid",
                           UintegerValue(4));
        
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpAdaptationAlgorithm"));
    }
    else if (burstGeneratorType == "model")
    {
        NS_LOG_UNCOND("Basic model-based burst generation selected");
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm", StringValue(""));
    }
    else
    {
        NS_LOG_UNCOND("Unsupported burstGeneratorType, defaulting to model");
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm", StringValue(""));
    }

    Ptr<NrEpcTft> vrTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter vrPf;
    vrPf.localPortStart = vrPort;
    vrPf.localPortEnd = vrPort;
    vrTft->Add(vrPf);
    // Add uplink filter to ensure UL flows match bearer for UDP
    NrEpcTft::PacketFilter vrPfUl;
    vrPfUl.remotePortStart = vrPort;
    vrPfUl.remotePortEnd = vrPort;
    vrTft->Add(vrPfUl);

    NrEpsBearer vrBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);

    // Server on remote host
    BurstyApplicationServerHelper vrServer("ns3::UdpSocketFactory",
                                           InetSocketAddress(Ipv4Address::GetAny(), vrPort));
    ApplicationContainer vrServerApp = vrServer.Install(remoteHost);
    vrServerApp.Start(Seconds(0.0));
    vrServerApp.Stop(simTime + udpAppStartTime + Seconds(1));

    NS_LOG_UNCOND("VR server installed on remote host");

    // Clients on UEs
    BurstyApplicationClientHelper vrClient("ns3::UdpSocketFactory",
                                           InetSocketAddress(remoteHostActualAddress, vrPort));
    ApplicationContainer vrClientApps = vrClient.Install(ueContainer);
    vrClientApps.Start(udpAppStartTime);
    vrClientApps.Stop(simTime + udpAppStartTime + Seconds(1));

    NS_LOG_UNCOND("VR client apps installed on " << ueContainer.GetN() << " UEs");

    // Bearers per UE - test with empty TFT (no filtering)
    for (uint32_t i = 0; i < ueContainer.GetN(); ++i)
    {
        Ptr<NetDevice> ueNetDev = ueDevs.Get(i);
        Ptr<Node> ueNode = ueNetDev->GetNode();
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        if (ipv4 && ipv4->GetNInterfaces() > 1)
        {
            Ipv4Address ueAddr = ipv4->GetAddress(1, 0).GetLocal();
            NS_LOG_UNCOND("UE " << i << " IP: " << ueAddr);

            // Set the "Local" attribute on the client to bind to this UE's IP
            Ptr<BurstyApplicationClient> app =
                DynamicCast<BurstyApplicationClient>(vrClientApps.Get(i));
            if (app)
            {
                app->SetAttribute("Local", AddressValue(InetSocketAddress(ueAddr, 0)));
            }
        }
        // Activate bearer with empty TFT (accepts all traffic)
        nr->ActivateDedicatedEpsBearer(ueDevs.Get(i), vrBearer, vrTft);
        NS_LOG_UNCOND("Bearer activated for UE " << i);
    }

    // ORAN infrastructure setup (for oran-util-udp)
    Ptr<OranNearRtRic> nearRtRic = nullptr;
    OranE2NodeTerminatorContainer e2NodeTerminatorsEnbs;
    OranE2NodeTerminatorContainer e2NodeTerminatorsUes;

    if (burstGeneratorType == "oran-util-udp")
    {
        Ptr<OranHelper> oranHelper = CreateObject<OranHelper>();

        // ORAN configuration
        Time lmQueryInterval = Seconds(5);
        Time maxWaitTime = Seconds(0.010);
        std::string dbFileName = "oran-vehicular.db";

        oranHelper->SetAttribute("Verbose", BooleanValue(false));
        oranHelper->SetAttribute("LmQueryInterval", TimeValue(lmQueryInterval));
        oranHelper->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(2)));
        oranHelper->SetAttribute("LmQueryMaxWaitTime", TimeValue(maxWaitTime));
        oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue("DROP"));

        // Setup data repository
        if (!dbFileName.empty())
        {
            std::remove(dbFileName.c_str());
        }
        oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite",
                                      "DatabaseFile",
                                      StringValue(dbFileName));
        oranHelper->SetDefaultLogicModule(
            "ns3::OranLmNr2NrRsrpHandover",
            "ProcessingDelayRv",
            StringValue("ns3::NormalRandomVariable[Mean=0.005|Variance=0.000031]"));
        oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");

        nearRtRic = oranHelper->CreateNearRtRic();

        // Connect VR bitrate logic module to data repository
        if (oranLogicVrBitrate != nullptr && nearRtRic != nullptr && nearRtRic->Data() != nullptr)
        {
            oranLogicVrBitrate->SetDataRepository(nearRtRic->Data());
        }

        // Setup gNB terminators
        oranHelper->SetE2NodeTerminator("ns3::OranE2NodeTerminatorNrGnb",
                                        "RegistrationIntervalRv",
                                        StringValue("ns3::ConstantRandomVariable[Constant=1]"),
                                        "SendIntervalRv",
                                        StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));

        // Set periodic reporting
        Config::SetDefault("ns3::OranReportTriggerPeriodic::IntervalRv",
                           StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));

        oranHelper->AddReporter("ns3::OranReporterLocation",
                                "Trigger",
                                StringValue("ns3::OranReportTriggerPeriodic"));
        oranHelper->AddReporter("ns3::OranReporterNrUeBitratePerLcid",
                                "Trigger",
                                StringValue("ns3::OranReportTriggerPeriodic"));
        oranHelper->AddReporter("ns3::OranReporterNrUeTxQueueSize",
                                "Trigger",
                                StringValue("ns3::OranReportTriggerPeriodic"));

        // Deploy gNB terminators
        OranE2NodeTerminatorContainer deployedEnbTerminators =
            oranHelper->DeployTerminators(nearRtRic, gnbContainer);
        e2NodeTerminatorsEnbs.Add(deployedEnbTerminators);

        // Connect gNB reporters to schedulers
        for (uint32_t idx = 0; idx < gnbDevs.GetN(); ++idx)
        {
            Ptr<NetDevice> dev = gnbDevs.Get(idx);
            Ptr<NrGnbNetDevice> gnbDevice = dev->GetObject<NrGnbNetDevice>();
            if (!gnbDevice)
            {
                continue;
            }

            Ptr<NrGnbMac> gnbMac = gnbDevice->GetMac(0);
            if (!gnbMac)
            {
                continue;
            }

            // Find the terminator for this node
            for (uint32_t t = 0; t < deployedEnbTerminators.GetN(); ++t)
            {
                Ptr<OranE2NodeTerminator> term = deployedEnbTerminators.Get(t);
                if (term->GetNode() == dev->GetNode())
                {
                    ObjectVectorValue reportersVal;
                    term->GetAttribute("Reporters", reportersVal);
                    for (std::size_t r = 0; r < reportersVal.GetN(); ++r)
                    {
                        Ptr<OranReporterNrUeTxQueueSize> txq =
                            DynamicCast<OranReporterNrUeTxQueueSize>(reportersVal.Get(r));
                        if (txq)
                        {
                            gnbMac->TraceConnectWithoutContext(
                                "BufferStatusReportTrace",
                                MakeCallback(&OranReporterNrUeTxQueueSize::OnBufferStatusReport,
                                             txq));
                        }
                        Ptr<OranReporterNrUeBitratePerLcid> br =
                            DynamicCast<OranReporterNrUeBitratePerLcid>(reportersVal.Get(r));
                        if (br)
                        {
                            for (uint32_t q = 0; q < 1; ++q)
                            {
                                Ptr<NrMacScheduler> sched = nr->GetScheduler(dev, q);
                                if (sched)
                                {
                                    sched->TraceConnectWithoutContext(
                                        "SchedStats",
                                        MakeCallback(&OranReporterNrUeBitratePerLcid::OnSchedStats,
                                                     br));
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // Schedule ORAN activation (early to collect data before apps start)
        Simulator::Schedule(Seconds(0.1),
                            &OranHelper::ActivateAndStartNearRtRic,
                            oranHelper,
                            nearRtRic);
        Simulator::Schedule(Seconds(0.3),
                            &OranHelper::ActivateE2NodeTerminators,
                            oranHelper,
                            e2NodeTerminatorsEnbs);
    }

    // Flow monitor
    FlowMonitorHelper fmHelper;
    NodeContainer endpoints;
    endpoints.Add(remoteHost);
    endpoints.Add(ueContainer);
    Ptr<FlowMonitor> monitor = fmHelper.Install(endpoints);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(simTime+udpAppStartTime + Seconds(2));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double avgTput = 0.0;
    double avgDelay = 0.0;
    double duration = (simTime).GetSeconds();

    std::ofstream out;
    std::string filename = outputDir + "/" + simTag;
    out.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        out << "Flow " << it->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
            << t.destinationAddress << ":" << t.destinationPort << ")\n";
        out << "  Tx Bytes: " << it->second.txBytes << "\n";
        out << "  Rx Bytes: " << it->second.rxBytes << "\n";
        if (it->second.rxPackets > 0)
        {
            double tput = it->second.rxBytes * 8.0 / duration / 1e6;
            double delay = 1000.0 * it->second.delaySum.GetSeconds() / it->second.rxPackets;
            avgTput += tput;
            avgDelay += delay;
            out << "  Throughput: " << tput << " Mbps\n";
            out << "  Mean delay: " << delay << " ms\n";
        }
        else
        {
            out << "  Throughput: 0 Mbps\n";
            out << "  Mean delay: 0 ms\n";
        }
    }

    if (!stats.empty())
    {
        out << "\nMean flow throughput: " << (avgTput / stats.size()) << " Mbps\n";
        out << "Mean flow delay: " << (avgDelay / stats.size()) << " ms\n";
    }
    out.close();

    std::ifstream f(filename.c_str());
    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();
    return EXIT_SUCCESS;
}
