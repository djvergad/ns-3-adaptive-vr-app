// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @ingroup examples
 * @file cttc-nr-demo.cc
 * @brief A cozy, simple, NR demo (in a tutorial style)
 *
 * Notice: this entire program uses technical terms defined by the 3GPP TS 38.300 [1].
 *
 * This example describes how to setup a simulation using the 3GPP channel model from TR 38.901 [2].
 * This example consists of a simple grid topology, in which you
 * can choose the number of gNbs and UEs. Have a look at the possible parameters
 * to know what you can configure through the command line.
 *
 * With the default configuration, the example will create two flows that will
 * go through two different subband numerologies (or bandwidth parts). For that,
 * specifically, two bands are created, each with a single CC, and each CC containing
 * one bandwidth part.
 *
 * The example will print on-screen the end-to-end result of one (or two) flows,
 * as well as writing them on a file.
 *
 * \code{.unparsed}
$ ./ns3 run "cttc-nr-demo --PrintHelp"
    \endcode
 *
 */

// NOLINTBEGIN
// clang-format off

/**
 * Useful references that will be used for this tutorial:
 * [1] <a href="https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=3191">3GPP TS 38.300</a>
 * [2] <a href="https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=3173">3GPP channel model from TR 38.901</a>
 * [3] <a href="https://www.nsnam.org/docs/release/3.38/tutorial/html/tweaking.html#using-the-logging-module">ns-3 documentation</a>
 */

// clang-format on
// NOLINTEND

/*
 * Include part. Often, you will have to include the headers for an entire module;
 * do that by including the name of the module you need with the suffix "-module.h".
 */

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/bursty-application-server-instance.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/oran-module.h"
#include "ns3/point-to-point-module.h"

#include <cmath>

/*
 * Use, always, the namespace ns3. All the NR classes are inside such namespace.
 */
using namespace ns3;

/*
 * With this line, we will be able to see the logs of the file by enabling the
 * component "CttcNrDemo".
 * Further information on how logging works can be found in the ns-3 documentation [3].
 */
NS_LOG_COMPONENT_DEFINE("CttcNrDemo");

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

int
main(int argc, char* argv[])
{
    /*
     * Variables that represent the parameters we will accept as input by the
     * command line. Each of them is initialized with a default value, and
     * possibly overridden below when command-line arguments are parsed.
     */
    // Scenario parameters (that we will use inside this script):
    uint16_t gNbNum = 1;
    uint16_t ueNumPergNb = 2;
    bool logging = false;
    bool doubleOperationalBand = true;
    double farUeDistance = 50.0; // Distance in meters for far UEs from gNB
    std::string channelScenario = "UMa"; // Channel scenario: UMi, UMa, RMa
    bool enableShadowing = true; // Enable shadowing for realistic path loss

    // Traffic parameters (that we will use inside this script):
    uint32_t udpPacketSizeULL = 100;
    uint32_t udpPacketSizeBe = 1252;
    uint32_t lambdaULL = 10000;
    uint32_t lambdaBe = 10000;

    // Simulation parameters. Please don't use double to indicate seconds; use
    // ns-3 Time values which use integers to avoid portability issues.
    Time simTime = MilliSeconds(1000);
    Time udpAppStartTime = MilliSeconds(400);

    // NR parameters (Reference: 3GPP TR 38.901 V17.0.0 (Release 17)
    // Table 7.8-1 for the power and BW).
    // In this example the BW has been split into two BWPs
    // We will take the input from the command line, and then we
    // will pass them inside the NR module.
    uint16_t numerologyBwp1 = 4;
    double centralFrequencyBand1 = 28e9;
    double bandwidthBand1 = 50e6;
    uint16_t numerologyBwp2 = 2;
    double centralFrequencyBand2 = 28.2e9;
    double bandwidthBand2 = 50e6;
    double totalTxPower = 23; // Reduced from 35 to 23 dBm for better differentiation

    std::string appRate = "50Mbps";        // the app target data rate
    double frameRate = 60;                 // the app frame rate [FPS]
    std::string vrAppName = "VirusPopper"; // the app name
    std::string burstGeneratorType =
        "model"; // type of burst generator {"model", "trace", "deterministic"}

    // Where we will store the output files.
    std::string simTag = "default";
    std::string outputDir = "./";

    /*
     * From here, we instruct the ns3::CommandLine class of all the input parameters
     * that we may accept as input, as well as their description, and the storage
     * variable.
     */
    CommandLine cmd(__FILE__);

    cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology", gNbNum);
    cmd.AddValue("ueNumPergNb", "The number of UE per gNb in multiple-ue topology", ueNumPergNb);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("doubleOperationalBand",
                 "If true, simulate two operational bands with one CC for each band,"
                 "and each CC will have 1 BWP that spans the entire CC.",
                 doubleOperationalBand);
    cmd.AddValue("farUeDistance",
                 "Distance in meters of far UEs from gNB (near UEs are at ~1.5m)",
                 farUeDistance);
    cmd.AddValue("channelScenario",
                 "3GPP channel scenario: UMi (Urban Micro), UMa (Urban Macro), or RMa (Rural Macro)",
                 channelScenario);
    cmd.AddValue("enableShadowing",
                 "Enable shadowing in the channel model for realistic fading",
                 enableShadowing);
    cmd.AddValue("packetSizeUll",
                 "packet size in bytes to be used by ultra low latency traffic",
                 udpPacketSizeULL);
    cmd.AddValue("packetSizeBe",
                 "packet size in bytes to be used by best effort traffic",
                 udpPacketSizeBe);
    cmd.AddValue("lambdaUll",
                 "Number of UDP packets in one second for ultra low latency traffic",
                 lambdaULL);
    cmd.AddValue("lambdaBe",
                 "Number of UDP packets in one second for best effort traffic",
                 lambdaBe);
    cmd.AddValue("simulationTime", "Simulation time", simTime);
    cmd.AddValue("numerologyBwp1", "The numerology to be used in bandwidth part 1", numerologyBwp1);
    cmd.AddValue("centralFrequencyBand1",
                 "The system frequency to be used in band 1",
                 centralFrequencyBand1);
    cmd.AddValue("bandwidthBand1", "The system bandwidth to be used in band 1", bandwidthBand1);
    cmd.AddValue("numerologyBwp2", "The numerology to be used in bandwidth part 2", numerologyBwp2);
    cmd.AddValue("centralFrequencyBand2",
                 "The system frequency to be used in band 2",
                 centralFrequencyBand2);
    cmd.AddValue("bandwidthBand2", "The system bandwidth to be used in band 2", bandwidthBand2);
    cmd.AddValue("totalTxPower",
                 "total tx power that will be proportionally assigned to"
                 " bands, CCs and bandwidth parts depending on each BWP bandwidth ",
                 totalTxPower);
    cmd.AddValue("appRate", "the app target data rate", appRate);
    cmd.AddValue("frameRate", "the app frame rate [FPS]", frameRate);
    cmd.AddValue("vrAppName", "the app name", vrAppName);
    cmd.AddValue("burstGeneratorType",
                 "type of burst generator {\"model\", \"google\", \"fuzzy\"}",
                 burstGeneratorType);

    cmd.AddValue("simTag",
                 "tag to be appended to output filenames to distinguish simulation campaigns",
                 simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);

    // Parse the command line
    cmd.Parse(argc, argv);

    /*
     * Check if the frequency is in the allowed range.
     * If you need to add other checks, here is the best position to put them.
     */
    NS_ABORT_IF(centralFrequencyBand1 < 0.5e9 && centralFrequencyBand1 > 100e9);
    NS_ABORT_IF(centralFrequencyBand2 < 0.5e9 && centralFrequencyBand2 > 100e9);
    
    // Validate channel scenario
    if (channelScenario != "UMi" && channelScenario != "UMa" && channelScenario != "RMa")
    {
        NS_ABORT_MSG("Invalid channel scenario: " << channelScenario 
                     << ". Valid options are: UMi, UMa, RMa");
    }
    
    // Validate farUeDistance
    NS_ABORT_IF(farUeDistance < 10.0);
    NS_LOG_INFO("Far UE distance configured to: " << farUeDistance << " meters");

    /*
     * If the logging variable is set to true, enable the log of some components
     * through the code. The same effect can be obtained through the use
     * of the NS_LOG environment variable:
     *
     * export NS_LOG="UdpClient=level_info|prefix_time|prefix_func|prefix_node:UdpServer=..."
     *
     * Usually, the environment variable way is preferred, as it is more customizable,
     * and more expressive.
     */
    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
    }

    LogComponentEnableAll(LOG_PREFIX_ALL);
    LogComponentEnable("CttcNrDemo", LOG_LEVEL_ALL);
    // LogComponentEnable("BurstyApplicationServer", LOG_LEVEL_ALL);
    // LogComponentEnable("BurstyApplicationClient", LOG_LEVEL_ALL);
    // LogComponentEnable ("BurstyApplication", LOG_ALL);
    // LogComponentEnable ("VrAdaptiveBurstSink", LOG_ALL);
    // LogComponentEnable ("VrAdaptiveBurstyApplication", LOG_ALL);
    // LogComponentEnable ("BurstSinkTcp", LOG_ALL);
    // LogComponentEnable ("BurstyApplicationTcp", LOG_LEVEL_ALL);
    // LogComponentEnable ("VrAdaptiveBurstSinkTcp", LOG_DEBUG);
    // LogComponentEnable("VrAdaptiveBurstyApplicationTcp", LOG_INFO);
    // LogComponentEnable("FuzzyAlgorithmServer", LOG_ALL);
    // LogComponentEnable("AdaptationAlgorithmServer", LOG_ALL);

    // LogComponentEnable("BurstyApplicationClient", LOG_ALL);
    // LogComponentEnable("BurstyApplicationServer", LOG_ALL);
    // LogComponentEnable("BurstyApplicationServerInstance", LOG_ALL);
    // LogComponentEnable("OranCellUtilizationCollector", LOG_LEVEL_ALL);
    // LogComponentEnable("OranCellUtilizationAdaptationAlgorithm", LOG_LEVEL_ALL);
    // LogComponentEnable("OranCellUtilizationUdpAdaptationAlgorithm", LOG_LEVEL_ALL);
    LogComponentEnable("OranLogicVrBitrate", LOG_LEVEL_ALL);
    // LogComponentEnable("OranDataRepositorySqlite", LOG_LEVEL_ALL);
    // LogComponentEnable("NrRlc", LOG_LEVEL_ALL);
    // LogComponentEnable("NrRlcUm", LOG_LEVEL_ALL);
    // LogComponentEnable("NrGnbMac", LOG_LEVEL_ALL);

    // LogComponentEnable("NrGnbRrc", LOG_LEVEL_ALL);

    // LogComponentEnable("OranReportNrUeBitratePerLcid", LOG_LEVEL_ALL);
    // LogComponentEnable("OranReporterNrUeBitratePerLcid", LOG_LEVEL_ALL);

    /*
     * In general, attributes for the NR module are typically configured in NrHelper.  However, some
     * attributes need to be configured globally through the Config::SetDefault() method. Below is
     * an example: if you want to make the RLC buffer very large, you can pass a very large integer
     * here.
     */
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(9999999));

    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(simTime));

    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 23));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 23));

    /*
     * Create the scenario. In our examples, we heavily use helpers that setup
     * the gnbs and ue following a pre-defined pattern. Please have a look at the
     * GridScenarioHelper documentation to see how the nodes will be distributed.
     * 
     * MODIFIED: Instead of using GridScenarioHelper's automatic UE placement,
     * we manually position UEs: half near the gNB and half at a specified distance
     * arranged in a circle.
     */
    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    // All units below are in meters
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNumPergNb * gNbNum);
    
    // Set a larger scenario size to accommodate far UEs
    double scenarioSize = std::max(farUeDistance * 2.5, 100.0);
    gridScenario.SetScenarioHeight(scenarioSize);
    gridScenario.SetScenarioLength(scenarioSize);
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    // Manually reposition UEs: half near gNB, half at farUeDistance in a circle
    NodeContainer allUes = gridScenario.GetUserTerminals();
    uint32_t totalUes = allUes.GetN();
    uint32_t nearUes = totalUes / 2;
    uint32_t farUes = totalUes - nearUes;
    
    // Get gNB position (assuming single gNB at index 0)
    Ptr<Node> gnbNode = gridScenario.GetBaseStations().Get(0);
    Ptr<MobilityModel> gnbMobility = gnbNode->GetObject<MobilityModel>();
    Vector gnbPos = gnbMobility->GetPosition();
    
    NS_LOG_INFO("gNB position: (" << gnbPos.x << ", " << gnbPos.y << ", " << gnbPos.z << ")");
    NS_LOG_INFO("Placing " << nearUes << " UEs near gNB and " << farUes << " UEs at distance " 
                << farUeDistance << "m");
    
    // Position near UEs in a small circle around the gNB (radius ~2m)
    double nearRadius = 2.0;
    for (uint32_t i = 0; i < nearUes; ++i)
    {
        Ptr<Node> ueNode = allUes.Get(i);
        Ptr<MobilityModel> ueMobility = ueNode->GetObject<MobilityModel>();
        
        double angle = (2.0 * M_PI * i) / nearUes;
        double x = gnbPos.x + nearRadius * cos(angle);
        double y = gnbPos.y + nearRadius * sin(angle);
        double z = 1.5; // UE height
        
        Vector newPos(x, y, z);
        ueMobility->SetPosition(newPos);
        NS_LOG_INFO("Near UE " << i << " positioned at (" << x << ", " << y << ", " << z << ")");
    }
    
    // Position far UEs in a circle at farUeDistance from the gNB
    for (uint32_t i = 0; i < farUes; ++i)
    {
        Ptr<Node> ueNode = allUes.Get(nearUes + i);
        Ptr<MobilityModel> ueMobility = ueNode->GetObject<MobilityModel>();
        
        double angle = (2.0 * M_PI * i) / farUes;
        double x = gnbPos.x + farUeDistance * cos(angle);
        double y = gnbPos.y + farUeDistance * sin(angle);
        double z = 1.5; // UE height
        
        Vector newPos(x, y, z);
        ueMobility->SetPosition(newPos);
        NS_LOG_INFO("Far UE " << (nearUes + i) << " positioned at (" << x << ", " << y << ", " << z << ")");
    }

    /*
     * Create two different NodeContainer for the different traffic type.
     * In ueLowLat we will put the UEs that will receive low-latency traffic,
     * while in ueVoice we will put the UEs that will receive the voice traffic.
     */
    NodeContainer ueLowLatContainer;
    NodeContainer ueVoiceContainer;

    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        if (j % 1 == 0)
        {
            ueLowLatContainer.Add(ue);
        }
        else
        {
            ueVoiceContainer.Add(ue);
        }
    }

    /*
     * TODO: Add a print, or a plot, that shows the scenario.
     */
    NS_LOG_INFO("Creating " << gridScenario.GetUserTerminals().GetN() << " user terminals and "
                            << gridScenario.GetBaseStations().GetN() << " gNBs");

    /*
     * Setup the NR module. We create the various helpers needed for the
     * NR simulation:
     * - nrEpcHelper, which will setup the core network
     * - IdealBeamformingHelper, which takes care of the beamforming part
     * - NrHelper, which takes care of creating and connecting the various
     * part of the NR stack
     * - NrChannelHelper, which takes care of the spectrum channel
     */
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Put the pointers inside nrHelper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaPF"));

    /*
     * Spectrum division. We create two operational bands, each of them containing
     * one component carrier, and each CC containing a single bandwidth part
     * centered at the frequency specified by the input parameters.
     * Each spectrum part length is, as well, specified by the input parameters.
     * Both operational bands will use the StreetCanyon channel modeling.
     */
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; // in this example, both bands have a single CC

    // Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
    // a single BWP per CC
    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1,
                                                    bandwidthBand1,
                                                    numCcPerBand);

    // Create the band and install the channel into it
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    // Set the channel for the band
    CcBwpCreator::SimpleOperationBandConf bandConf2(centralFrequencyBand2,
                                                    bandwidthBand2,
                                                    numCcPerBand);
    OperationBandInfo band2 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf2);

    /*
     * The configured spectrum division is:
     * ------------Band1--------------|--------------Band2-----------------
     * ------------CC1----------------|--------------CC2-------------------
     * ------------BWP1---------------|--------------BWP2------------------
     */

    /*
     * Start to account for the bandwidth used by the example, as well as
     * the total power that has to be divided among the BWPs.
     */
    double x = pow(10, totalTxPower / 10);
    double totalBandwidth = bandwidthBand1;
    /**
     * The channel is configured by this helper using a combination of the scenario, the channel
     * condition model, and the fading model.
     */

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories(channelScenario, "Default", "ThreeGpp");
    /**
     * Use channelHelper API to define the attributes for the channel model (condition, pathloss and
     * spectrum)
     * 
     * MODIFIED: Using configurable channel scenario and shadowing to create realistic
     * path loss differences between near and far UEs.
     * - UMa (Urban Macro): Higher path loss, suitable for larger cell sizes
     * - RMa (Rural Macro): Even higher path loss over distance
     * - Shadowing adds realistic fading variability
     */
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(enableShadowing));
    
    NS_LOG_INFO("Using channel scenario: " << channelScenario 
                << ", Shadowing: " << (enableShadowing ? "Enabled" : "Disabled")
                << ", TX Power: " << totalTxPower << " dBm");
    /*
     * if not single band simulation, initialize and setup power in the second band.
     * Install channel and pathloss, plus other things inside single or both bands.
     */
    if (doubleOperationalBand)
    {
        channelHelper->AssignChannelsToBands({band1, band2});
        totalBandwidth += bandwidthBand2;
        allBwps = CcBwpCreator::GetAllBwps({band1, band2});
    }
    else
    {
        channelHelper->AssignChannelsToBands({band1});
        allBwps = CcBwpCreator::GetAllBwps({band1});
    }

    /*
     * allBwps contains all the spectrum configuration needed for the nrHelper.
     *
     * Now, we can setup the attributes. We can have three kind of attributes:
     * (i) parameters that are valid for all the bandwidth parts and applies to
     * all nodes, (ii) parameters that are valid for all the bandwidth parts
     * and applies to some node only, and (iii) parameters that are different for
     * every bandwidth parts. The approach is:
     *
     * - for (i): Configure the attribute through the helper, and then install;
     * - for (ii): Configure the attribute through the helper, and then install
     * for the first set of nodes. Then, change the attribute through the helper,
     * and install again;
     * - for (iii): Install, and then configure the attributes by retrieving
     * the pointer needed, and calling "SetAttribute" on top of such pointer.
     *
     */

    // Packet::EnableChecking();
    // Packet::EnablePrinting();

    /*
     *  Case (i): Attributes valid for all the nodes
     */
    // Beamforming method
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // Core latency
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    // Antennas for all the UEs
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Antennas for all the gNbs
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    uint32_t bwpIdForLowLat = 0;
    uint32_t bwpIdForVoice = 0;
    if (doubleOperationalBand)
    {
        bwpIdForVoice = 1;
        bwpIdForLowLat = 0;
    }

    // gNb routing between Bearer and bandwidh part
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB",
                                                 UintegerValue(bwpIdForLowLat));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));

    // Ue routing between Bearer and bandwidth part
    nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB", UintegerValue(bwpIdForLowLat));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));

    /*
     * We miss many other parameters. By default, not configuring them is equivalent
     * to use the default values. Please, have a look at the documentation to see
     * what are the default values for all the attributes you are not seeing here.
     */

    /*
     * Case (ii): Attributes valid for a subset of the nodes
     */

    // NOT PRESENT IN THIS SIMPLE EXAMPLE

    /*
     * We have configured the attributes we needed. Now, install and get the pointers
     * to the NetDevices, which contains all the NR stack:
     */

    NetDeviceContainer gnbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueLowLatNetDev = nrHelper->InstallUeDevice(ueLowLatContainer, allBwps);
    NetDeviceContainer ueVoiceNetDev = nrHelper->InstallUeDevice(ueVoiceContainer, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueLowLatNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVoiceNetDev, randomStream);
    /*
     * Case (iii): Go node for node and change the attributes we have to setup
     * per-node.
     */

    // Get the first netdevice (gnbNetDev.Get (0)) and the first bandwidth part (0)
    // and set the attribute.
    NrHelper::GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    NrHelper::GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidthBand1 / totalBandwidth) * x)));

    if (doubleOperationalBand)
    {
        // Get the first netdevice (gnbNetDev.Get (0)) and the second bandwidth part (1)
        // and set the attribute.
        NrHelper::GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetAttribute("Numerology", UintegerValue(numerologyBwp2));
        NrHelper::GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetTxPower(10 * log10((bandwidthBand2 / totalBandwidth) * x));
    }

    // From here, it is standard NS3. In the future, we will create helpers
    // for this part as well.

    auto [remoteHost, remoteHostIpv4Address] =
        nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.000));

    // Get the actual IP address of the remote host node
    Ptr<Ipv4> remoteHostIpv4 = remoteHost->GetObject<Ipv4>();
    Ipv4Address remoteHostActualAddress = remoteHostIpv4Address;
    if (remoteHostIpv4)
    {
        // Get the first interface's IP (skip loopback which is interface 0)
        for (uint32_t i = 1; i < remoteHostIpv4->GetNInterfaces(); ++i)
        {
            Ipv4InterfaceAddress ifAddr = remoteHostIpv4->GetAddress(i, 0);
            Ipv4Address addr = ifAddr.GetLocal();
            // Check if this is not 127.0.0.1
            if (addr != Ipv4Address("127.0.0.1"))
            {
                remoteHostActualAddress = addr;
                break;
            }
        }
    }
    NS_LOG_UNCOND("Remote host address: " << remoteHostActualAddress
                                          << " (Gateway: " << remoteHostIpv4Address << ")");

    InternetStackHelper internet;

    internet.Install(gridScenario.GetUserTerminals());

    Ipv4InterfaceContainer ueLowLatIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLowLatNetDev));
    Ipv4InterfaceContainer ueVoiceIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVoiceNetDev));

    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    for (uint32_t u = 0; u < ueLowLatContainer.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueLowLatContainer.Get(u);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        if (ipv4)
        {
            Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
            ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
        }
    }

    // Also add routes for voice UEs
    for (uint32_t u = 0; u < ueVoiceContainer.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueVoiceContainer.Get(u);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        if (ipv4)
        {
            Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
            ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
        }
    }

    // attach UEs to the closest gNB
    nrHelper->AttachToClosestGnb(ueLowLatNetDev, gnbNetDev);
    nrHelper->AttachToClosestGnb(ueVoiceNetDev, gnbNetDev);

    /*
     * Traffic part. Install two kind of traffic: low-latency and voice, each
     * identified by a particular source port.
     */
    uint16_t dlPortLowLat = 1234;
    uint16_t dlPortVoice = 1235;

    /*
     * Configure attributes for the different generators, using user-provided
     * parameters for generating a CBR traffic
     *
     * Low-Latency configuration and object creation:
     */
    UdpClientHelper dlClientLowLat;
    dlClientLowLat.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientLowLat.SetAttribute("PacketSize", UintegerValue(udpPacketSizeULL));
    dlClientLowLat.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaULL)));

    // Ptr<ns3::OranInMemoryDataRepository> repo = CreateObject<ns3::OranInMemoryDataRepository>();
    Ptr<ns3::OranLogicVrBitrate> oranLogicVrBitrate = CreateObject<ns3::OranLogicVrBitrate>();

    // ORAN Models -- Initialize RIC BEFORE application setup
    // This ensures oranLogicVrBitrate is properly initialized with the data repository
    // before the adaptation algorithm instances are created

    std::string dbFileName = "oran-repository.db";
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
    oranHelper->SetAttribute("LmQueryMaxWaitTime",
                             TimeValue(maxWaitTime)); // 0 means wait for all LMs to finish
    oranHelper->SetAttribute("LmQueryLateCommandPolicy", StringValue(lateCommandPolicy));

    // RIC setup
    if (!dbFileName.empty())
    {
        std::remove(dbFileName.c_str());
    }

    oranHelper->SetDataRepository("ns3::OranDataRepositorySqlite",
                                  "DatabaseFile",
                                  StringValue(dbFileName));
    oranHelper->SetDefaultLogicModule("ns3::OranLmNr2NrRsrpHandover",
                                      "ProcessingDelayRv",
                                      StringValue(processingDelayRv));
    oranHelper->SetConflictMitigationModule("ns3::OranCmmNoop");

    nearRtRic = oranHelper->CreateNearRtRic();

    // Connect the VR bitrate logic module to the data repository managed by the
    // Near-RT RIC and register it so it participates in LM queries.
    if (oranLogicVrBitrate != nullptr && nearRtRic != nullptr && nearRtRic->Data() != nullptr)
    {
        oranLogicVrBitrate->SetDataRepository(nearRtRic->Data());
        // nearRtRic->AddLogicModule(oranLogicVrBitrate);
    }

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
        // Config::SetDefault("ns3::OranCellUtilizationAdaptationAlgorithm::collector",
        //                    PointerValue(Ptr<OranCellUtilizationCollector>(collector)));
    }
    else if (burstGeneratorType == "oran-util-udp")
    {
        protocol = "ns3::UdpSocketFactory";
        // Use UDP-based ORAN utilization adaptation algorithm
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpAdaptationAlgorithm"));
        // Provide the collector instance so the algorithm can query cell utilization
        Config::SetDefault("ns3::OranCellUtilizationUdpAdaptationAlgorithm::OranLogicVrBitrate",
                           PointerValue(Ptr<OranLogicVrBitrate>(oranLogicVrBitrate)));
    }
    else if (burstGeneratorType == "oran-util-udp-no-queue")
    {
        protocol = "ns3::UdpSocketFactory";
        // Use UDP-based ORAN utilization adaptation algorithm
        Config::SetDefault("ns3::BurstyApplicationServer::adaptationAlgorithm",
                           StringValue("OranCellUtilizationUdpNoQueueAdaptationAlgorithm"));
        // Provide the collector instance so the algorithm can query cell utilization
        Config::SetDefault("ns3::OranCellUtilizationUdpNoQueueAdaptationAlgorithm::OranLogicVrBitrate",
                           PointerValue(Ptr<OranLogicVrBitrate>(oranLogicVrBitrate)));
    }
    else
    {
        NS_ABORT_MSG("Wrong burstGeneratorType type");
    }

    // The bearer that will carry low latency traffic
    NrEpsBearer lowLatBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);

    // The filter for the low-latency traffic
    Ptr<NrEpcTft> lowLatTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfLowLat;
    dlpfLowLat.localPortStart = dlPortLowLat;
    dlpfLowLat.localPortEnd = dlPortLowLat;
    lowLatTft->Add(dlpfLowLat);
    // Also add uplink filter for same port
    if (protocol != "ns3::TcpSocketFactory")
    {
        NrEpcTft::PacketFilter ulpfLowLat;
        ulpfLowLat.remotePortStart = dlPortLowLat;
        ulpfLowLat.remotePortEnd = dlPortLowLat;
        lowLatTft->Add(ulpfLowLat);
    }
    // Voice configuration and object creation:
    UdpClientHelper dlClientVoice;
    dlClientVoice.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientVoice.SetAttribute("PacketSize", UintegerValue(udpPacketSizeBe));
    dlClientVoice.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaBe)));

    // // The bearer that will carry voice traffic
    NrEpsBearer voiceBearer(NrEpsBearer::GBR_CONV_VOICE);

    // The filter for the voice traffic
    Ptr<NrEpcTft> voiceTft = Create<NrEpcTft>();
    // NrEpcTft::PacketFilter dlpfVoice;
    // dlpfVoice.localPortStart = dlPortVoice;
    // dlpfVoice.localPortEnd = dlPortVoice;
    // voiceTft->Add(dlpfVoice);
    // Also add uplink filter for same port
    NrEpcTft::PacketFilter ulpfVoice;
    ulpfVoice.remotePortStart = dlPortLowLat;
    ulpfVoice.remotePortEnd = dlPortLowLat;
    voiceTft->Add(ulpfVoice);

    /*OranReporterNrUeBitratePerLcid
     * Let's install the applications!
     */

    uint16_t port = dlPortLowLat;

    uint32_t fragmentSize = 1200; // bytes

    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate", DataRateValue(DataRate(appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(vrAppName));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(fragmentSize));

    BurstyApplicationServerHelper server(protocol, InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer serverApp = server.Install(remoteHost);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simTime + Seconds(3));

    BurstyApplicationClientHelper client(protocol,
                                         InetSocketAddress(remoteHostActualAddress, port));
    ApplicationContainer clientApps = client.Install(ueLowLatContainer);
    Ptr<UniformRandomVariable> randomStart =
        CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                          DoubleValue(2.5),
                                                          "Max",
                                                          DoubleValue(3));

    // Setup traces
    AsciiTraceHelper ascii;

    Ptr<OutputStreamWrapper> burstTrace = ascii.CreateFileStream("burstTrace.csv");
    *burstTrace->GetStream() << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,BurstSize" << std::endl;
    Ptr<OutputStreamWrapper> fragmentTrace = ascii.CreateFileStream("fragmentTrace.csv");
    *fragmentTrace->GetStream()
        << "SrcAddress,TxTime_ns,RxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize" << std::endl;

    // Also log server-side fragment transmissions to a separate CSV so we can
    // correlate which fragments were sent versus which were received.
    Ptr<OutputStreamWrapper> txFragmentTrace = ascii.CreateFileStream("txFragmentTrace.csv");
    *txFragmentTrace->GetStream()
        << "DstAddress,EventTime_ns,TxTime_ns,BurstSeq,FragSeq,TotFrags,FragSize" << std::endl;
    if (serverApp.GetN() > 0)
    {
        serverApp.Get(0)->TraceConnectWithoutContext(
            "FragmentRx",
            MakeBoundCallback(&FragmentRx, txFragmentTrace));
    }

    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    {
        Time startTime = Seconds(randomStart->GetValue());
        NS_LOG_UNCOND("STA" << i << " will start at " << startTime.As(Time::S));
        Ptr<BurstyApplicationClient> app = DynamicCast<BurstyApplicationClient>(clientApps.Get(i));

        app->SetStartTime(startTime);
        app->SetAttribute("Local",
                          AddressValue(InetSocketAddress(ueLowLatIpIface.GetAddress(i), 0)));

        app->TraceConnectWithoutContext("BurstRx", MakeBoundCallback(&BurstRx, burstTrace));
        app->TraceConnectWithoutContext("FragmentRx",
                                        MakeBoundCallback(&FragmentRx, fragmentTrace));
    }

    clientApps.Stop(simTime + Seconds(3));

    // Activate dedicated bearers for low-latency traffic through BurstyApplicationClient
    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    {
        Ptr<NetDevice> ueDevice = ueLowLatNetDev.Get(i);
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, lowLatBearer, lowLatTft);
    }

    // Install UDP servers on UEs to receive downlink traffic
    UdpServerHelper ulServer1(dlPortLowLat);
    ApplicationContainer ulServerApps1 = ulServer1.Install(ueLowLatContainer);
    ulServerApps1.Start(Seconds(0.0));
    ulServerApps1.Stop(simTime + Seconds(3));

    UdpServerHelper ulServer2(dlPortVoice);
    ApplicationContainer ulServerApps2 = ulServer2.Install(ueVoiceContainer);
    ulServerApps2.Start(Seconds(0.0));
    ulServerApps2.Stop(simTime + Seconds(3));

    // Create downlink UDP clients to send traffic from remote host to UEs
    // ApplicationContainer dlClientAppsLowLat;
    // for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    // {
    //     Ipv4Address ueAddress = ueLowLatIpIface.GetAddress(i);
    //     dlClientLowLat.SetAttribute(
    //         "Remote",
    //         AddressValue(InetSocketAddress(ueAddress, dlPortLowLat)));
    //     dlClientAppsLowLat.Add(dlClientLowLat.Install(remoteHost));
    // }
    // dlClientAppsLowLat.Start(udpAppStartTime);
    // dlClientAppsLowLat.Stop(simTime);

    // ApplicationContainer dlClientAppsVoice;
    // for (uint32_t i = 0; i < ueVoiceContainer.GetN(); ++i)
    // {
    //     Ipv4Address ueAddress = ueVoiceIpIface.GetAddress(i);
    //     dlClientVoice.SetAttribute(
    //         "Remote",
    //         AddressValue(InetSocketAddress(ueAddress, dlPortVoice)));
    //     dlClientAppsVoice.Add(dlClientVoice.Install(remoteHost));
    // }
    // dlClientAppsVoice.Start(udpAppStartTime);
    // dlClientAppsVoice.Stop(simTime);

    // Create uplink UDP clients to send traffic from UEs to remote host
    // UdpClientHelper ulClientLowLat;
    // ulClientLowLat.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    // ulClientLowLat.SetAttribute("PacketSize", UintegerValue(udpPacketSizeULL));
    // ulClientLowLat.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaULL)));

    // uint16_t ulPortLowLat = 5001; // Uplink port
    // ApplicationContainer ulClientAppsLowLat;
    // for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    // {
    //     ulClientLowLat.SetAttribute(
    //         "Remote",
    //         AddressValue(InetSocketAddress(remoteHostActualAddress, ulPortLowLat)));
    //     ulClientAppsLowLat.Add(ulClientLowLat.Install(ueLowLatContainer.Get(i)));
    // }
    // ulClientAppsLowLat.Start(udpAppStartTime);
    // ulClientAppsLowLat.Stop(simTime);

    // Install UDP servers on remote host to receive uplink traffic
    // UdpServerHelper ulServerRemoteLowLat(ulPortLowLat);
    // ApplicationContainer ulServerRemoteAppsLowLat = ulServerRemoteLowLat.Install(remoteHost);
    // ulServerRemoteAppsLowLat.Start(Seconds(0.0));
    // ulServerRemoteAppsLowLat.Stop(simTime + Seconds(3));

    // ApplicationContainer clientApps;

    // for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    // {
    //     Ptr<Node> ue = ueLowLatContainer.Get(i);
    //     Ptr<NetDevice> ueDevice = ueLowLatNetDev.Get(i);
    //     Address ueAddress = ueLowLatIpIface.GetAddress(i);

    //     // The client, who is transmitting, is installed in the remote host,
    //     // with destination address set to the address of the UE
    //     dlClientLowLat.SetAttribute(
    //         "Remote",
    //         AddressValue(addressUtils::ConvertToSocketAddress(ueAddress, dlPortLowLat)));
    //     clientApps.Add(dlClientLowLat.Install(remoteHost));

    //     // Activate a dedicated bearer for the traffic type
    //     nrHelper->ActivateDedicatedEpsBearer(ueDevice, lowLatBearer, lowLatTft);
    // }

    // for (uint32_t i = 0; i < ueVoiceContainer.GetN(); ++i)
    // {
    //     Ptr<Node> ue = ueVoiceContainer.Get(i);
    //     Ptr<NetDevice> ueDevice = ueVoiceNetDev.Get(i);
    //     Address ueAddress = ueVoiceIpIface.GetAddress(i);

    //     // The client, who is transmitting, is installed in the remote host,
    //     // with destination address set to the address of the UE
    //     dlClientVoice.SetAttribute(
    //         "Remote",
    //         AddressValue(addressUtils::ConvertToSocketAddress(ueAddress, dlPortVoice)));
    //     clientApps.Add(dlClientVoice.Install(remoteHost));

    //     // Activate a dedicated bearer for the traffic type
    //     nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
    // }

    // // start UDP server and client apps
    // serverApps.Start(udpAppStartTime);
    // clientApps.Start(udpAppStartTime);
    // serverApps.Stop(simTime);
    // clientApps.Stop(simTime);

    // enable the traces provided by the nr module
    // nrHelper->EnableTraces();

    // ORAN Models -- UE and ENB Node Setup
    // UE Nodes setup
    for (uint32_t idx = 0; idx < gridScenario.GetUserTerminals().GetN(); idx++)
    {
        Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
        Ptr<OranReporterNrUeCellInfo> nrUeCellInfoReporter =
            CreateObject<OranReporterNrUeCellInfo>();
        Ptr<OranReporterNrUeRsrpRsrq> rsrpRsrqReporter = CreateObject<OranReporterNrUeRsrpRsrq>();
        Ptr<OranE2NodeTerminatorNrUe> nrUeTerminator = CreateObject<OranE2NodeTerminatorNrUe>();

        locationReporter->SetAttribute("Terminator", PointerValue(nrUeTerminator));

        nrUeCellInfoReporter->SetAttribute("Terminator", PointerValue(nrUeTerminator));

        rsrpRsrqReporter->SetAttribute("Terminator", PointerValue(nrUeTerminator));

        for (uint32_t netDevIdx = 0;
             netDevIdx < gridScenario.GetUserTerminals().Get(idx)->GetNDevices();
             netDevIdx++)
        {
            Ptr<NrUeNetDevice> nrUeDevice = gridScenario.GetUserTerminals()
                                                .Get(idx)
                                                ->GetDevice(netDevIdx)
                                                ->GetObject<NrUeNetDevice>();
            if (nrUeDevice)
            {
                Ptr<NrUePhy> uePhy = nrUeDevice->GetPhy(0);
                uePhy->TraceConnectWithoutContext(
                    "ReportUeMeasurements",
                    MakeCallback(&ns3::OranReporterNrUeRsrpRsrq::ReportRsrpRsrq, rsrpRsrqReporter));
            }
        }

        nrUeTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
        nrUeTerminator->SetAttribute("RegistrationIntervalRv",
                                     StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        nrUeTerminator->SetAttribute("SendIntervalRv",
                                     StringValue("ns3::ConstantRandomVariable[Constant=1]"));

        nrUeTerminator->AddReporter(locationReporter);
        nrUeTerminator->AddReporter(nrUeCellInfoReporter);
        nrUeTerminator->AddReporter(rsrpRsrqReporter);

        nrUeTerminator->Attach(gridScenario.GetUserTerminals().Get(idx));

        Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorNrUe::Activate, nrUeTerminator);
    }

    // ENb Nodes setup
    oranHelper->SetE2NodeTerminator("ns3::OranE2NodeTerminatorNrGnb",
                                    "RegistrationIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=1]"),
                                    "SendIntervalRv",
                                    StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));

    // Set the reporting period to 200ms for the periodic trigger used by reporters
    Config::SetDefault("ns3::OranReportTriggerPeriodic::IntervalRv",
                       StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));

    oranHelper->AddReporter("ns3::OranReporterLocation",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    // Register a bitrate reporter factory with the helper so DeployTerminators will
    // create one per deployed ENB terminator.
    oranHelper->AddReporter("ns3::OranReporterNrUeBitratePerLcid",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));
    oranHelper->AddReporter("ns3::OranReporterNrUeTxQueueHolDelay",
                            "Trigger",
                            StringValue("ns3::OranReportTriggerPeriodic"));

    std::cout << "Deploying ENB terminators and wiring reporters" << std::endl;

    // Deploy terminators for ENBs using the helper. This returns the created
    // terminators and also installs them so they will be activated by
    // ActivateE2NodeTerminators.
    OranE2NodeTerminatorContainer deployedEnbTerminators =
        oranHelper->DeployTerminators(nearRtRic, gridScenario.GetBaseStations());

    // Add deployed terminators to the container that will be activated later.
    e2NodeTerminatorsEnbs.Add(deployedEnbTerminators);

    // Connect each gNB MAC BufferStatusReportTrace to the corresponding
    // OranReporterNrUeBitratePerLcid instance created by the terminator.
    if (burstGeneratorType == "oran-util-udp" ||
        burstGeneratorType == "oran-util-udp-no-queue")
    {
        for (uint32_t idx = 0; idx < gnbNetDev.GetN(); ++idx)
        {
            Ptr<NetDevice> dev = gnbNetDev.Get(idx);
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

            // Find the terminator attached to this node
            for (uint32_t t = 0; t < deployedEnbTerminators.GetN(); ++t)
            {
                Ptr<OranE2NodeTerminator> term = deployedEnbTerminators.Get(t);
                if (term->GetNode() == dev->GetNode())
                {
                    // Retrieve reporters attribute and connect any bitrate reporter
                    ObjectVectorValue reportersVal;
                    term->GetAttribute("Reporters", reportersVal);
                    for (std::size_t r = 0; r < reportersVal.GetN(); ++r)
                    {
                        Ptr<Object> repObj = reportersVal.Get(r);
                        Ptr<OranReporterNrUeBitratePerLcid> br =
                            DynamicCast<OranReporterNrUeBitratePerLcid>(repObj);
                        if (br)
                        {
                            // gnbMac->TraceConnectWithoutContext(
                            //     "BufferStatusReportTrace",
                            //     MakeCallback(&OranReporterNrUeBitratePerLcid::OnBufferStatusReport,
                            //     br));
                            // Also attach to scheduler SchedStats trace (if available)

                            // std::cout << "Connecting SchedStats trace for bitrate reporter" << "
                            // t= "
                            // << t << std::endl;
                            for (uint32_t q = 0; q < 2; ++q)
                            {
                                std::cout << " Node devices[" << q << "]=" << std::endl;

                                Ptr<NrMacScheduler> sched = nrHelper->GetScheduler(dev, q);
                                if (sched)
                                {
                                    sched->TraceConnectWithoutContext(
                                        "SchedStats",
                                        MakeCallback(&OranReporterNrUeBitratePerLcid::OnSchedStats,
                                                     br));
                                }
                            }
                        }
                        Ptr<OranReporterNrUeTxQueueHolDelay> txq =
                            DynamicCast<OranReporterNrUeTxQueueHolDelay>(repObj);
                        if (txq)
                        {
                            gnbMac->TraceConnectWithoutContext(
                                "BufferStatusReportTrace",
                                MakeCallback(&OranReporterNrUeTxQueueHolDelay::OnBufferStatusReport,
                                             txq));
                        }
                    }
                    break; // found the terminator for this node
                }
            }
        }
    }

    // DB logging to the terminal
    // if (dbLog)
    // {
    // nearRtRic->Data()->TraceConnectWithoutContext("QueryRc", MakeCallback(&QueryRcSink));
    // }

    // Activate and the components
    Simulator::Schedule(Seconds(1), &OranHelper::ActivateAndStartNearRtRic, oranHelper, nearRtRic);
    Simulator::Schedule(Seconds(1.5),
                        &OranHelper::ActivateE2NodeTerminators,
                        oranHelper,
                        e2NodeTerminatorsEnbs);
    Simulator::Schedule(Seconds(2),
                        &OranHelper::ActivateE2NodeTerminators,
                        oranHelper,
                        e2NodeTerminatorsUes);

    // collector->SetDataRepository();
    // Initialize collector so it connects to DlScheduling traces
    // collector->DoInitialize();

    // ORAN Models -- END (all initialization now completed before applications)

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(gridScenario.GetUserTerminals());

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(simTime + Seconds(4));
    Simulator::Run();

    /*
     * To check what was installed in the memory, i.e., BWPs of gNB Device, and its configuration.
     * Example is: Node 1 -> Device 0 -> BandwidthPartMap -> {0,1} BWPs -> NrGnbPhy -> Numerology,
    GtkConfigStore config;
    config.ConfigureAttributes ();
    */

    // Print per-flow statistics
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

    double flowDuration = (simTime - udpAppStartTime).GetSeconds();
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
            // Measure the duration of the flow from receiver's perspective
            averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
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

    // burst info
    Ptr<OutputStreamWrapper> txBurstsBySta = ascii.CreateFileStream("txBurstsBySta.csv");
    Ptr<OutputStreamWrapper> rxBursts = ascii.CreateFileStream("rxBursts.csv");

    uint64_t totBurstSent = 0;
    for (auto& kv : (DynamicCast<BurstyApplicationServer>(serverApp.Get(0)))->GetInstances())
    {
        BurstyApplicationServerInstance& serverInstance = kv.second;
        uint64_t burstsSent = serverInstance.GetTotalTxBursts();
        totBurstSent += burstsSent;
        std::cout << "burstsSent(" << kv.first << ")=" << burstsSent << ", ";
        *txBurstsBySta->GetStream() << burstsSent << std::endl;
    }

    uint64_t burstsReceived = 0;
    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); i++)
    {
        burstsReceived +=
            DynamicCast<BurstyApplicationClient>(clientApps.Get(i))->GetTotalRxBursts();
    }

    std::cout << "burstsReceived=" << burstsReceived << " ("
              << double(burstsReceived) / totBurstSent * 100 << "%)" << std::endl;
    *rxBursts->GetStream() << burstsReceived << std::endl;

    // fragment info
    Ptr<OutputStreamWrapper> txFragmentsBySta = ascii.CreateFileStream("txFragmentsBySta.csv");
    Ptr<OutputStreamWrapper> rxFragments = ascii.CreateFileStream("rxFragments.csv");

    uint64_t totFragmentSent = 0;
    for (auto& kv : (DynamicCast<BurstyApplicationServer>(serverApp.Get(0)))->GetInstances())
    {
        BurstyApplicationServerInstance& serverInstance = kv.second;
        uint64_t fragmentsSent = serverInstance.GetTotalTxFragments();
        totFragmentSent += fragmentsSent;
        std::cout << "fragmentsSent(" << kv.first << ")=" << fragmentsSent << ", ";
        *txFragmentsBySta->GetStream() << fragmentsSent << std::endl;
    }

    uint64_t fragmentsReceived = 0;
    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); i++)
    {
        fragmentsReceived +=
            DynamicCast<BurstyApplicationClient>(clientApps.Get(i))->GetTotalRxFragments();
    }

    std::cout << "fragmentsReceived=" << fragmentsReceived << " ("
              << double(fragmentsReceived) / totFragmentSent * 100 << "%)" << std::endl;
    *rxFragments->GetStream() << fragmentsReceived << std::endl;

    Simulator::Destroy();

    if (argc == 0)
    {
        double toleranceMeanFlowThroughput = 0.0001 * 56.258560;
        double toleranceMeanFlowDelay = 0.0001 * 0.553292;

        if (meanFlowThroughput >= 56.258560 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 56.258560 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 0.553292 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 0.553292 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else if (argc == 1 and ueNumPergNb == 9) // called from examples-to-run.py with these parameters
    {
        double toleranceMeanFlowThroughput = 0.0001 * 47.858536;
        double toleranceMeanFlowDelay = 0.0001 * 10.504189;

        if (meanFlowThroughput >= 47.858536 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 47.858536 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 10.504189 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 10.504189 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        return EXIT_SUCCESS; // we dont check other parameters configurations at the moment
    }
}
