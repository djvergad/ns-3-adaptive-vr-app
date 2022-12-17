/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 SIGNET Lab, Department of Information Engineering,
 * University of Padova
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iomanip>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "ns3/seq-ts-size-frag-header.h"
// #include "ns3/bursty-helper.h"
// #include "ns3/burst-sink-helper.h"
#include "ns3/bursty-application-client-helper.h"
#include "ns3/bursty-application-server-helper.h"
#include "ns3/vr-burst-generator.h"

using namespace ns3;

/**
 * An example of synthetic traces for VR applications.
 * For further information please check the reference paper (see README.md).
 */

NS_LOG_COMPONENT_DEFINE ("Test1");

std::string
AddressToString (const Address &addr)
{
  std::stringstream addressStr;
  addressStr << InetSocketAddress::ConvertFrom (addr).GetIpv4 () << ":"
             << InetSocketAddress::ConvertFrom (addr).GetPort ();
  return addressStr.str ();
}

void
BurstRx (Ptr<const Packet> burst, const Address &from, const Address &to,
         const SeqTsSizeFragHeader &header)
{
  NS_LOG_INFO ("Received burst seq=" << header.GetSeq () << " of " << header.GetSize ()
                                     << " bytes transmitted at " << std::setprecision (9)
                                     << header.GetTs ().As (Time::S));
}

int
main (int argc, char *argv[])
{
  double simTime = 500;
  double frameRate = 30;
  std::string appRate = "40Mbps";
  std::string vrAppName = "VirusPopper";
  std::string burstGeneratorType = "model";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameRate", "VR application frame rate [FPS].", frameRate);
  cmd.AddValue ("appRate", "Target data rate of the VR application.", appRate);
  cmd.AddValue ("vrAppName", "The VR application on which the model is based upon.", vrAppName);
  cmd.AddValue ("simTime", "Length of simulation [s].", simTime);
  cmd.AddValue("burstGeneratorType",
              "type of burst generator {\"model\", \"adaptive\"}",
                 burstGeneratorType);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnable ("Test1", LOG_INFO);
  LogComponentEnable ("BurstyApplicationClient", LOG_ALL);
  LogComponentEnable ("BurstyApplicationServer", LOG_ALL);
  // LogComponentEnable ("BurstyApplicationServerInstance", LOG_ALL);

      LogComponentEnable("TcpTxBuffer", LOG_LEVEL_ALL);


    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));

    Config::SetDefault("ns3::TcpL4Protocol::RecoveryType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpPrrRecovery")));

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpHtcp")));
// Config::SetDefault ("ns3::TcpSocketBase::UseEcn", StringValue ("On"));
    // Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue (MilliSeconds (20)));
Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
        


    Config::SetDefault("ns3::VrBurstGenerator::FrameRate", DoubleValue(frameRate));
    Config::SetDefault("ns3::VrBurstGenerator::TargetDataRate", DataRateValue(DataRate(appRate)));
    Config::SetDefault("ns3::VrBurstGenerator::VrAppName", StringValue(vrAppName));
    Config::SetDefault("ns3::BurstyApplicationServer::FragmentSize", UintegerValue(500));

    Config::SetDefault("ns3::BurstyApplicationServer::appDuration", TimeValue(Seconds(simTime)));



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
    else
    {
        NS_ABORT_MSG("Wrong burstGeneratorType type");
    }





  // Setup two nodes
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("200Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("50ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  uint16_t portNumber = 50000;

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4Address serverAddress = interfaces.GetAddress (0);
  Ipv4Address sinkAddress = Ipv4Address::GetAny (); // 0.0.0.0

  // Config::SetDefault ("ns3::BurstyApplicationServerInstance::FragmentSize", UintegerValue (1200));

  // Create burst sink helper
  BurstyApplicationServerHelper burstyApplicationServerHelper (
      protocol, InetSocketAddress (sinkAddress, portNumber));

  // Install bursty application
  ApplicationContainer serverApps = burstyApplicationServerHelper.Install (nodes.Get (0));
  Ptr<BurstyApplicationServer> burstyApp =
      serverApps.Get (0)->GetObject<BurstyApplicationServer> ();


  // Create bursty application helper
  BurstyApplicationClientHelper burstyApplicationClientHelper (
      protocol, InetSocketAddress (serverAddress, portNumber));
  // burstyHelper.SetAttribute ("FragmentSize", UintegerValue (1200));

  // burstyHelper.SetBurstGenerator ("ns3::VrBurstGenerator",
  //                                 "FrameRate", DoubleValue (frameRate),
  //                                 "appRate", DataRateValue (DataRate (appRate)),
  //                                 "VrAppName", StringValue(vrAppName));

  // Install HTTP client
  ApplicationContainer clientApps = burstyApplicationClientHelper.Install (nodes.Get (1));
  Ptr<BurstyApplicationClient> burstSink =
      clientApps.Get (0)->GetObject<BurstyApplicationClient> ();


  // Example of connecting to the trace sources
  burstSink->TraceConnectWithoutContext ("BurstRx", MakeCallback (&BurstRx));

  clientApps.Start (Seconds (1.0));

  // Stop bursty app after simTime
  clientApps.Stop (Seconds (simTime + 1));

  serverApps.Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  // Stats
  std::cout << "Total RX bursts: " << burstyApp->GetTotalTxBursts () << "/"
            << burstSink->GetTotalRxBursts () << std::endl;
  std::cout << "Total RX fragments: " << burstyApp->GetTotalTxFragments () << "/"
            << burstSink->GetTotalRxFragments () << std::endl;
  std::cout << "Total RX bytes: " << burstyApp->GetTotalTxBytes () << "/"
            << burstSink->GetTotalRxBytes () << std::endl;

  return 0;
}
