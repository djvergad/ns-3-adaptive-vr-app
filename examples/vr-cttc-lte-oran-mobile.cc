// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de
// Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/*
 * LTE variant of the VR + ORAN example with mobile UEs and hexagonal ENB placement.
 * This file extends vr-cttc-lte-oran.cc with:
 * - ENBs placed in a hexagonal grid with configurable distance
 * - UEs placed randomly in a topology where all are within range of at least one ENB
 * - UEs moving with random velocity and direction (configurable max velocity)
 * - UEs reflected back when reaching topology boundaries
@@ * 
@@ * NOTE: The oran-util-udp adaptation algorithm may produce warnings about unmapped
@@ * E2 node IDs due to UE mobility. The OranLogicVrBitrate module was designed for
@@ * static topologies. Use --burstGeneratorType=model or other non-ORAN algorithms
@@ * for best results with mobile UEs.
 */

#include "ns3/applications-module.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/bursty-application-server-instance.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/oran-module.h"
#include "ns3/oran-reporter-lte-ue-tx-queue-size.h"
#include "ns3/oran-reporter-lte-ue-bitrate-per-lcid.h"
#include "ns3/point-to-point-module.h"
#include "ns3/buildings-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CttcLteDemoMobile");

std::vector<std::string>
SplitString(const std::string& str, char delimiter)
{
    std::stringstream ss(str);
    std::string token;
    std::vector<std::string> container;

    while (getline(ss, token, delimiter))
    {
        container.push_back(token);
    }
    return container;
}

std::string
AddressToString(const Address& addr)
{
    std::stringstream addressStr;
    addressStr << InetSocketAddress::ConvertFrom(addr).GetIpv4();
    return addressStr.str();
}

void
BurstRx(Ptr<OutputStreamWrapper> traceFile,
        Ptr<const Packet> burst,
        const Address& from,
        const Address& to,
        const SeqTsSizeFragHeader& header)
{
    *traceFile->GetStream() << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
                            << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << ","
                            << header.GetSize() << "\n";
}

void
FragmentRx(Ptr<OutputStreamWrapper> traceFile,
           Ptr<const Packet> fragment,
           const Address& from,
           const Address& to,
           const SeqTsSizeFragHeader& header)
{
    *traceFile->GetStream() << AddressToString(to) << "," << header.GetTs().GetNanoSeconds() << ","
                            << Simulator::Now().GetNanoSeconds() << "," << header.GetSeq() << ","
                            << header.GetFragSeq() << "," << header.GetFrags() << ","
                            << fragment->GetSize() << "\n";
}

void
QueryRcSink(std::string query, std::string args, int rc)
{
    std::cout << Simulator::Now().GetSeconds() << " Query "
              << ((rc == SQLITE_OK || rc == SQLITE_DONE) ? "OK" : "ERROR") << "(" << rc << "): \""
              << query << "\"";

    if (!args.empty())
    {
        std::cout << " (" << args << ")";
    }
    std::cout << std::endl;
}

/**
 * Helper to manage constant velocity mobility with boundary reflection
 */
class MobileNodeController : public SimpleRefCount<MobileNodeController>
{
public:
    MobileNodeController(Ptr<Node> node, double minX, double maxX, double minY, double maxY,
                         double height, double maxSpeed, double minSpeed)
        : m_node(node),
          m_minX(minX),
          m_maxX(maxX),
          m_minY(minY),
          m_maxY(maxY),
          m_height(height),
          m_maxSpeed(maxSpeed),
          m_minSpeed(minSpeed)
    {
        m_randSpeed = CreateObject<UniformRandomVariable>();
        m_randAngle = CreateObject<UniformRandomVariable>();
        m_randChangeDir = CreateObject<UniformRandomVariable>();
        m_randSpeed->SetAttribute("Min", DoubleValue(m_minSpeed));
        m_randSpeed->SetAttribute("Max", DoubleValue(m_maxSpeed));
        m_randAngle->SetAttribute("Min", DoubleValue(0.0));
        m_randAngle->SetAttribute("Max", DoubleValue(2.0 * M_PI));
        m_randChangeDir->SetAttribute("Min", DoubleValue(0.0));
        m_randChangeDir->SetAttribute("Max", DoubleValue(1.0));
    }

    void ScheduleNextWalk(Time startTime = Seconds(0.0))
    {
        Simulator::Schedule(startTime, &MobileNodeController::DoWalk, this);
    }

private:
    void DoWalk(void)
    {
        Ptr<MobilityModel> mobility = m_node->GetObject<MobilityModel>();
        if (!mobility)
            return;

        Vector currentPos = mobility->GetPosition();
        Vector currentVel = DynamicCast<ConstantVelocityMobilityModel>(mobility)->GetVelocity();

        // Check for boundary violations and reflect velocity
        double vx = currentVel.x;
        double vy = currentVel.y;

        // If we're near or beyond boundaries, reflect the velocity component
        if (currentPos.x <= m_minX || currentPos.x >= m_maxX)
        {
            vx = -vx; // Reverse X velocity
        }
        if (currentPos.y <= m_minY || currentPos.y >= m_maxY)
        {
            vy = -vy; // Reverse Y velocity
        }

        // Sometimes change direction randomly (20% chance)
        if (m_randChangeDir->GetValue() < 0.2)
        {
            double speed = std::sqrt(vx * vx + vy * vy);
            if (speed < 0.1)
            {
                speed = m_randSpeed->GetValue();
            }
            double angle = m_randAngle->GetValue();
            vx = speed * std::cos(angle);
            vy = speed * std::sin(angle);
        }

        // Set the constant velocity
        Ptr<ConstantVelocityMobilityModel> cvMobility =
            DynamicCast<ConstantVelocityMobilityModel>(mobility);
        if (cvMobility)
        {
            cvMobility->SetVelocity(Vector(vx, vy, 0.0));
        }

        // Schedule next velocity update
        Simulator::Schedule(Seconds(1.0), &MobileNodeController::DoWalk, this);
    }

    Ptr<Node> m_node;
    double m_minX, m_maxX, m_minY, m_maxY, m_height;
    double m_maxSpeed, m_minSpeed;
    Ptr<UniformRandomVariable> m_randSpeed, m_randAngle, m_randChangeDir;
};

/**
 * Helper class to generate hexagonal grid of positions
 */
class HexagonalGridGenerator
{
public:
    /**
     * Generate hexagonal grid positions
     * @param numCells Number of cells per side (creates roughly hexagonalCells^2 positions)
     * @param distance Distance between adjacent eNBs
     * @param centerX Center X coordinate
     * @param centerY Center Y coordinate
     * @param height Height for all positions
     * @return Vector of positions
     */
    static std::vector<Vector>
    GenerateHexagonalPositions(uint32_t numCells, double distance, double centerX, double centerY,
                               double height)
    {
        std::vector<Vector> positions;
        const double sqrt3 = std::sqrt(3.0);

        for (int row = -static_cast<int>(numCells); row <= static_cast<int>(numCells); ++row)
        {
            for (int col = -static_cast<int>(numCells); col <= static_cast<int>(numCells); ++col)
            {
                double x, y;

                if (row % 2 == 0)
                {
                    x = centerX + col * distance * sqrt3 / 2.0;
                }
                else
                {
                    x = centerX + (col + 0.5) * distance * sqrt3 / 2.0;
                }
                y = centerY + row * distance * 0.75;

                positions.push_back(Vector(x, y, height));
            }
        }

        return positions;
    }

    /**
     * Calculate bounding box for hexagonal grid
     * @param numCells Number of cells per side
     * @param distance Distance between adjacent eNBs
     * @param centerX Center X coordinate
     * @param centerY Center Y coordinate
     * @param enbCoverageRadius Coverage radius of each eNB
     * @param minX Output: minimum X
     * @param maxX Output: maximum X
     * @param minY Output: minimum Y
     * @param maxY Output: maximum Y
     */
    static void
    GetBoundingBox(uint32_t numCells, double distance, double centerX, double centerY,
                   double enbCoverageRadius, double& minX, double& maxX, double& minY, double& maxY)
    {
        const double sqrt3 = std::sqrt(3.0);
        double maxGridX = centerX + numCells * distance * sqrt3 / 2.0;
        double maxGridY = centerY + numCells * distance * 0.75;

        minX = centerX - maxGridX + enbCoverageRadius;
        maxX = maxGridX - centerX + enbCoverageRadius;
        minY = centerY - maxGridY + enbCoverageRadius;
        maxY = maxGridY - centerY + enbCoverageRadius;
    }
};

int
main(int argc, char* argv[])
{
    uint16_t hexGridSize = 2; // Number of cells per side in hexagonal grid
    double enbDistance = 100.0; // Distance between ENBs in meters
    uint16_t ueNum = 10; // Total number of UEs
    double maxUeVelocity = 5.0; // Maximum UE velocity in m/s
    bool logging = false;

    uint32_t udpPacketSizeULL = 100;
    uint32_t udpPacketSizeBe = 1252;
    uint32_t lambdaULL = 10000;
    uint32_t lambdaBe = 10000;

    Time simTime = MilliSeconds(1000);
    Time udpAppStartTime = MilliSeconds(400);

    std::string appRate = "50Mbps";
    double frameRate = 60;
    std::string vrAppName = "VirusPopper";
    std::string burstGeneratorType = "model";

    std::string simTag = "default";
    std::string outputDir = "./";

    CommandLine cmd(__FILE__);
    cmd.AddValue("hexGridSize", "Number of cells per side in hexagonal ENB grid", hexGridSize);
    cmd.AddValue("enbDistance", "Distance between adjacent ENBs in meters", enbDistance);
    cmd.AddValue("ueNum", "Total number of UEs", ueNum);
    cmd.AddValue("maxUeVelocity", "Maximum UE velocity in m/s", maxUeVelocity);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("packetSizeUll", "packet size in bytes to be used by ultra low latency traffic",
                 udpPacketSizeULL);
    cmd.AddValue("packetSizeBe", "packet size in bytes to be used by best effort traffic",
                 udpPacketSizeBe);
    cmd.AddValue("lambdaUll", "Number of UDP packets in one second for ultra low latency traffic",
                 lambdaULL);
    cmd.AddValue("lambdaBe", "Number of UDP packets in one second for best effort traffic", lambdaBe);
    cmd.AddValue("simulationTime", "Simulation time", simTime);
    cmd.AddValue("appRate", "the app target data rate", appRate);
    cmd.AddValue("frameRate", "the app frame rate [FPS]", frameRate);
    cmd.AddValue("vrAppName", "the app name", vrAppName);
    cmd.AddValue("burstGeneratorType",
                 "type of burst generator {\"model\", \"google\", \"fuzzy\", \"bola\", \"mpc\", "
                 "\"festive\", \"oran-util\", \"oran-util-udp\", \"oran-util-udp-no-queue\"}",
                 burstGeneratorType);
    cmd.AddValue("simTag", "tag to be appended to output filenames to distinguish simulation campaigns",
                 simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);
    cmd.Parse(argc, argv);

    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    LogComponentEnableAll(LOG_PREFIX_ALL);
    LogComponentEnable("OranLogicVrBitrate", LOG_LEVEL_ALL);

    // Set EpsBearer to Release 15 to support NGBR_LOW_LAT_EMBB QCI
    Config::SetDefault("ns3::EpsBearer::Release", UintegerValue(15));

    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(simTime));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 23));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 23));

    // LTE-A RLC Configuration for improved throughput with multiple carriers
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(99999999));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(99999999));
    // RLC AM timer configuration to handle mobility/congestion and reduce packet loss
    Config::SetDefault("ns3::LteRlcAm::PollRetransmitTimer", TimeValue(MilliSeconds(50)));
    Config::SetDefault("ns3::LteRlcAm::ReorderingTimer", TimeValue(MilliSeconds(40)));
    Config::SetDefault("ns3::LteRlcAm::StatusProhibitTimer", TimeValue(MilliSeconds(10)));

    int64_t randomStream = 1;

    // Generate hexagonal grid for ENBs
    std::vector<Vector> enbPositions =
        HexagonalGridGenerator::GenerateHexagonalPositions(hexGridSize, enbDistance, 0.0, 0.0, 10.0);
    uint16_t gNbNum = enbPositions.size();

    NS_LOG_INFO("Generating hexagonal grid with " << gNbNum << " ENBs");

    // Create ENB nodes
    NodeContainer enbNodes;
    enbNodes.Create(gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    // Place ENBs in hexagonal grid
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> enbPos = CreateObject<ListPositionAllocator>();
    for (const auto& pos : enbPositions)
    {
        enbPos->Add(pos);
    }
    mobility.SetPositionAllocator(enbPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // Calculate coverage bounds for UE placement
    double minX, maxX, minY, maxY;
    double enbCoverageRadius = 250.0; // Typical LTE eNB coverage radius
    HexagonalGridGenerator::GetBoundingBox(hexGridSize, enbDistance, 0.0, 0.0, enbCoverageRadius,
                                            minX, maxX, minY, maxY);

    NS_LOG_INFO("UE placement bounds: X=[" << minX << ", " << maxX << "], Y=[" << minY << ", "
                                           << maxY << "]");

    // Place UEs randomly within coverage area
    Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable>();
    randX->SetAttribute("Min", DoubleValue(minX));
    randX->SetAttribute("Max", DoubleValue(maxX));
    randY->SetAttribute("Min", DoubleValue(minY));
    randY->SetAttribute("Max", DoubleValue(maxY));

    Ptr<ListPositionAllocator> uePos = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < ueNum; ++i)
    {
        double x = randX->GetValue();
        double y = randY->GetValue();
        uePos->Add(Vector(x, y, 1.5));
    }

    // Set up mobile UE movement with reflection at boundaries
    mobility.SetPositionAllocator(uePos);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    mobility.Install(ueNodes);

    // Set up velocity controllers for boundary reflection and random direction changes
    std::vector<Ptr<MobileNodeController>> controllers;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<MobileNodeController> controller = Create<MobileNodeController>(
            ueNodes.Get(i), minX, maxX, minY, maxY, 1.5, maxUeVelocity, 0.5);
        controllers.push_back(controller);
        controller->ScheduleNextWalk(Seconds(0.0));
    }

    NodeContainer ueLowLatContainer = ueNodes;
    NodeContainer ueVoiceContainer;

    // All UEs are low latency in this configuration
    // (keeping the structure similar to original for compatibility)

    NS_LOG_INFO("Creating " << ueNodes.GetN() << " user terminals and " << enbNodes.GetN()
                            << " eNBs");

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // LTE-A Configuration: Enable Carrier Aggregation
    uint16_t numberOfComponentCarriers = 2;
    lteHelper->SetAttribute("NumberOfComponentCarriers", UintegerValue(numberOfComponentCarriers));
    lteHelper->SetAttribute("EnbComponentCarrierManager", StringValue("ns3::RrComponentCarrierManager"));
    lteHelper->SetAttribute("UeComponentCarrierManager",
                            StringValue("ns3::SimpleUeComponentCarrierManager"));

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Configure handover algorithm for faster, smoother handovers during mobility
    lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(1.0));  // 1 dB hysteresis
    lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(64)));  // Faster trigger

    // Set LTE transmission power - increased eNB power for better coverage during mobility
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(46.0));  // Increased from 30 to 46 dBm
    Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(23.0));

    NetDeviceContainer enbNetDev = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLowLatNetDev = lteHelper->InstallUeDevice(ueLowLatContainer);

    randomStream += lteHelper->AssignStreams(enbNetDev, randomStream);
    randomStream += lteHelper->AssignStreams(ueLowLatNetDev, randomStream);
    // Add X2 interfaces between all eNBs to enable handover
    lteHelper->AddX2Interface(enbNodes);


    // Remote host / PGW setup
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(65000));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    std::cout << "Remote host address: " << internetIpIfaces.GetAddress(1) << std::endl;

    // Install internet stack on UEs
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueLowLatIpIface = epcHelper->AssignUeIpv4Address(ueLowLatNetDev);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Configure routing for remote host to reach UE network
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    for (uint32_t u = 0; u < ueLowLatContainer.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueLowLatContainer.Get(u);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        if (ipv4)
        {
            Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
            ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        }
    }

    // Attach UEs to the closest eNB
    lteHelper->AttachToClosestEnb(ueLowLatNetDev, enbNetDev);

    // Traffic configuration
    uint16_t dlPortLowLat = 1234;

    UdpClientHelper dlClientLowLat;
    dlClientLowLat.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientLowLat.SetAttribute("PacketSize", UintegerValue(udpPacketSizeULL));
    dlClientLowLat.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaULL)));

    Ptr<ns3::OranLogicVrBitrate> oranLogicVrBitrate = CreateObject<ns3::OranLogicVrBitrate>();

    std::string protocol;
    if (burstGeneratorType == "model")
    {
        protocol = "ns3::UdpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm", StringValue(""));
    }
    else if (burstGeneratorType == "google")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("GoogleAlgorithmServer"));
    }
    else if (burstGeneratorType == "fuzzy")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("FuzzyAlgorithmServer"));
    }
    else if (burstGeneratorType == "bola")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("BolaAlgo"));
    }
    else if (burstGeneratorType == "mpc")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("MPCAlgo"));
    }
    else if (burstGeneratorType == "festive")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("FestiveAlgorithm"));
    }
    else if (burstGeneratorType == "oran-util")
    {
        protocol = "ns3::TcpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationAdaptationAlgorithm"));
    }
    else if (burstGeneratorType == "oran-util-udp")
    {
        protocol = "ns3::UdpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpAdaptationAlgorithm"));
        Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::OranLogicVrBitrate",
                           PointerValue(Ptr<OranLogicVrBitrate>(oranLogicVrBitrate)));
    }
    else if (burstGeneratorType == "oran-util-udp-no-queue")
    {
        protocol = "ns3::UdpSocketFactory";
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpNoQueueAdaptationAlgorithm"));
        Config::SetDefault(
            "ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::OranLogicVrBitrate",
            PointerValue(Ptr<OranLogicVrBitrate>(oranLogicVrBitrate)));
    }
    else
    {
        NS_ABORT_MSG("Wrong burstGeneratorType type");
    }

    // LTE bearer and TFT configuration
    EpsBearer lowLatBearer(EpsBearer::NGBR_LOW_LAT_EMBB);
    lowLatBearer.SetRelease(15);
    Ptr<EpcTft> lowLatTft = Create<EpcTft>();
    EpcTft::PacketFilter dlpfLowLat;
    dlpfLowLat.localPortStart = dlPortLowLat;
    dlpfLowLat.localPortEnd = dlPortLowLat;
    lowLatTft->Add(dlpfLowLat);
    if (protocol != "ns3::TcpSocketFactory")
    {
        EpcTft::PacketFilter ulpfLowLat;
        ulpfLowLat.remotePortStart = dlPortLowLat;
        ulpfLowLat.remotePortEnd = dlPortLowLat;
        lowLatTft->Add(ulpfLowLat);
    }

    uint16_t port = dlPortLowLat;
    uint32_t fragmentSize = 1472;

    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate", DataRateValue(DataRate(appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(vrAppName));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(fragmentSize));

    BurstyApplicationServerHelper server(protocol, InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = server.Install(remoteHost);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simTime + Seconds(3));

    BurstyApplicationClientHelper client(
        protocol, InetSocketAddress(internetIpIfaces.GetAddress(1), port));
    ApplicationContainer clientApps = client.Install(ueLowLatContainer);

    // Increase startup delay to ensure ORAN E2 node terminators are fully registered
    // before applications start sending data (avoids adaptation algorithm errors)
    Ptr<UniformRandomVariable> randomStart =
        CreateObjectWithAttributes<UniformRandomVariable>("Min", DoubleValue(3.5), "Max",
                                                          DoubleValue(4.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> burstTrace = ascii.CreateFileStream("burstTrace-lte-mobile.csv");
    *burstTrace->GetStream() << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,BurstSize" << std::endl;
    Ptr<OutputStreamWrapper> fragmentTrace = ascii.CreateFileStream("fragmentTrace-lte-mobile.csv");
    *fragmentTrace->GetStream() << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize"
                               << std::endl;
    Ptr<OutputStreamWrapper> txFragmentTrace =
        ascii.CreateFileStream("txFragmentTrace-lte-mobile.csv");
    *txFragmentTrace->GetStream() << "DstAddress,EventTime_ns,TxTime_ns,BurstSeq,FragSeq,TotFrags,"
                                  "FragSize" << std::endl;
    if (serverApp.GetN() > 0)
    {
        serverApp.Get(0)->TraceConnectWithoutContext("FragmentRx",
                                                      MakeBoundCallback(&FragmentRx, txFragmentTrace));
    }

    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    {
        Time startTime = Seconds(randomStart->GetValue());
        NS_LOG_UNCOND("UE" << i << " will start at " << startTime.As(Time::S));
        Ptr<BurstyApplicationClient> app = DynamicCast<BurstyApplicationClient>(clientApps.Get(i));

        app->SetStartTime(startTime);
        app->SetAttribute("Local", AddressValue(InetSocketAddress(ueLowLatIpIface.GetAddress(i), 0)));

        app->TraceConnectWithoutContext("BurstRx", MakeBoundCallback(&BurstRx, burstTrace));
        app->TraceConnectWithoutContext("FragmentRx", MakeBoundCallback(&FragmentRx, fragmentTrace));
    }

    clientApps.Stop(simTime + Seconds(3));

    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    {
        Ptr<NetDevice> ueDevice = ueLowLatNetDev.Get(i);
        lteHelper->ActivateDedicatedEpsBearer(ueDevice, lowLatBearer, lowLatTft);
    }

    UdpServerHelper ulServer1(dlPortLowLat);
    ApplicationContainer ulServerApps1 = ulServer1.Install(ueLowLatContainer);
    ulServerApps1.Start(Seconds(0.0));
    ulServerApps1.Stop(simTime + Seconds(3));

    // ORAN Models
    std::string dbFileName = "oran-repository-lte-mobile.db";
    Time lmQueryInterval = Seconds(5);
    Time maxWaitTime = Seconds(0.010);
    std::string lateCommandPolicy = "DROP";
    std::string processingDelayRv = "ns3::NormalRandomVariable[Mean=0.005|Variance=0.000031]";

    Ptr<OranNearRtRic> nearRtRic = nullptr;
    OranE2NodeTerminatorContainer e2NodeTerminatorsEnbs;
    OranE2NodeTerminatorContainer e2NodeTerminatorsUes;
    Ptr<OranHelper> oranHelper = CreateObject<OranHelper>();

    oranHelper->SetAttribute("Verbose", BooleanValue(true));
    oranHelper->SetAttribute("LmQueryInterval", TimeValue(lmQueryInterval));
    oranHelper->SetAttribute("E2NodeInactivityThreshold", TimeValue(Seconds(2)));
    oranHelper->SetAttribute("E2NodeInactivityIntervalRv",
                             StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    oranHelper->SetAttribute("LmQueryMaxWaitTime", TimeValue(maxWaitTime));
    oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue(lateCommandPolicy));

    Config::SetDefault("ns3::OranReportTriggerPeriodic::IntervalRv",
                       StringValue("ns3::ConstantRandomVariable[Constant=0.05]"));

    if (!dbFileName.empty())
    {
        std::remove(dbFileName.c_str());
    }

    oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite", "DatabaseFile",
                                  StringValue(dbFileName));
    oranHelper->SetDefaultLogicModule("ns3::OranLmNr2NrRsrpHandover", "ProcessingDelayRv",
                                      StringValue(processingDelayRv));
    oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");

    nearRtRic = oranHelper->CreateNearRtRic();

    if (oranLogicVrBitrate != nullptr && nearRtRic != nullptr && nearRtRic->Data() != nullptr)
    {
        oranLogicVrBitrate->SetDataRepository(nearRtRic->Data());
    }

    // UE terminators and reporters
    for (uint32_t idx = 0; idx < ueNodes.GetN(); idx++)
    {
        Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
        Ptr<OranReporterLteUeCellInfo> lteUeCellInfoReporter =
            CreateObject<OranReporterLteUeCellInfo>();
        Ptr<OranReporterNrUeRsrpRsrq> rsrpRsrqReporter = CreateObject<OranReporterNrUeRsrpRsrq>();
        Ptr<OranReporterAppLoss> appLossReporter = CreateObject<OranReporterAppLoss>();
        Ptr<OranE2NodeTerminatorLteUe> lteUeTerminator = CreateObject<OranE2NodeTerminatorLteUe>();

        locationReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));
        lteUeCellInfoReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));
        rsrpRsrqReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));

        lteUeTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
        lteUeTerminator->SetAttribute("RegistrationIntervalRv",
                                      StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        lteUeTerminator->SetAttribute("SendIntervalRv",
                                      StringValue("ns3::ConstantRandomVariable[Constant=1]"));

        lteUeTerminator->AddReporter(locationReporter);
        lteUeTerminator->AddReporter(lteUeCellInfoReporter);
        lteUeTerminator->AddReporter(rsrpRsrqReporter);
        lteUeTerminator->AddReporter(appLossReporter);

        lteUeTerminator->Attach(ueNodes.Get(idx));
        Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorLteUe::Activate, lteUeTerminator);
        e2NodeTerminatorsUes.Add(lteUeTerminator);
    }

    // ENB terminators
    for (uint32_t idx = 0; idx < enbNetDev.GetN(); ++idx)
    {
        Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
        Ptr<OranReporterLteUeTxQueueSize> txQueueReporter =
            CreateObject<OranReporterLteUeTxQueueSize>();
        Ptr<OranReporterLteUeBitratePerLcid> bitrateReporter =
            CreateObject<OranReporterLteUeBitratePerLcid>();
        Ptr<OranE2NodeTerminatorLteEnb> lteEnbTerminator =
            CreateObject<OranE2NodeTerminatorLteEnb>();

        locationReporter->SetAttribute("Terminator", PointerValue(lteEnbTerminator));
        txQueueReporter->SetAttribute("Terminator", PointerValue(lteEnbTerminator));
        bitrateReporter->SetAttribute("Terminator", PointerValue(lteEnbTerminator));

        lteEnbTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
        lteEnbTerminator->SetAttribute("RegistrationIntervalRv",
                                       StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        lteEnbTerminator->SetAttribute("SendIntervalRv",
                                       StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));

        lteEnbTerminator->AddReporter(locationReporter);
        lteEnbTerminator->AddReporter(txQueueReporter);
        lteEnbTerminator->AddReporter(bitrateReporter);

        // Connect the eNB MAC BufferStatusReportTrace to the TX queue reporter
        Ptr<NetDevice> dev = enbNetDev.Get(idx);
        Ptr<LteEnbNetDevice> enbDevice = dev->GetObject<LteEnbNetDevice>();
        if (enbDevice)
        {
            Ptr<LteEnbMac> enbMac = enbDevice->GetMac();
            if (enbMac)
            {
                enbMac->TraceConnectWithoutContext(
                    "BufferStatusReportTrace",
                    MakeCallback(&OranReporterLteUeTxQueueSize::OnBufferStatusReport,
                                 txQueueReporter));
                // Connect DL and UL scheduling traces to the bitrate reporter
                enbMac->TraceConnectWithoutContext(
                    "DlScheduling",
                    MakeCallback(&OranReporterLteUeBitratePerLcid::OnDlScheduling, bitrateReporter));
                enbMac->TraceConnectWithoutContext(
                    "UlScheduling",
                    MakeCallback(&OranReporterLteUeBitratePerLcid::OnUlScheduling, bitrateReporter));
            }
        }

        lteEnbTerminator->Attach(enbNodes.Get(idx));
        Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorLteEnb::Activate, lteEnbTerminator);
        e2NodeTerminatorsEnbs.Add(lteEnbTerminator);
    }

    Simulator::Schedule(Seconds(1), &OranHelper::ActivateAndStartNearRtRic, oranHelper, nearRtRic);
    Simulator::Schedule(Seconds(1.5), &OranHelper::ActivateE2NodeTerminators, oranHelper,
                        e2NodeTerminatorsEnbs);
    Simulator::Schedule(Seconds(2), &OranHelper::ActivateE2NodeTerminators, oranHelper,
                        e2NodeTerminatorsUes);

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueNodes);

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(simTime + Seconds(4));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;

    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }
    outFile.setf(std::ios_base::fixed);

    double flowDuration = simTime.GetSeconds();
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
                << t.destinationAddress << ":" << t.destinationPort << ") proto " << protoStream.str()
                << "\n";
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

    double meanFlowThroughput = averageFlowThroughput / stats.size();
    double meanFlowDelay = averageFlowDelay / stats.size();

    outFile << "\n\n  Mean flow throughput: " << meanFlowThroughput << "\n";
    outFile << "  Mean flow delay: " << meanFlowDelay << "\n";
    outFile.close();

    std::ifstream f(filename.c_str());
    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();
    return EXIT_SUCCESS;
}
