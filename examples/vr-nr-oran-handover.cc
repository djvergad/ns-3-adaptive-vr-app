// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-nr-oran-handover.cc
 * @brief Downlink VR traffic to a moving UE that is handed over between NR gNBs by an O-RAN xApp.
 *
 * The scenario is described in vr-nr-oran-handover-scenario.h. Defaults: the moving UE only,
 * channel condition drawn randomly ("Default"), TDMA round robin scheduler, 23 dBm UE TX power,
 * TCP Cubic with 8 MB buffers for the TCP-based algorithms.
 * plot-vr-handover.py plots the results of a run with and without --handover.
 */

#include "vr-nr-oran-handover-scenario.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    vrnr::HandoverOptions options;
    options.backgroundUes = false;
    options.channelCondition = "Default";
    options.scheduler = "ns3::NrMacSchedulerTdmaRR";
    options.ueTxPower = 23;
    options.tcpType = "ns3::TcpCubic";
    options.tcpBufferSize = 1 << 23;
    options.outPrefix = "vr-handover";
    return vrnr::RunVrNrOranHandover(argc, argv, __FILE__, options);
}
