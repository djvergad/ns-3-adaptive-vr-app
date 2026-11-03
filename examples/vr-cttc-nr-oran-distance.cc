// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-cttc-nr-oran-distance.cc
 * @brief VR over NR with O-RAN: UEs on three rings (near, medium, far) around the gNB.
 *
 * Same scenario as vr-cttc-nr-oran.cc (see vr-cttc-nr-oran-scenario.h) with defaults that
 * create path loss differences between the UEs: rings placement, RMa channel with shadowing
 * and the channel condition updated every 100 ms.
 *
 * \code{.unparsed}
$ ./ns3 run "vr-cttc-nr-oran-distance --PrintHelp"
    \endcode
 */

#include "vr-cttc-nr-oran-scenario.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    vrnr::CttcNrOranOptions options;
    options.uePlacement = "rings";
    options.channelScenario = "RMa";
    options.enableShadowing = true;
    options.channelUpdatePeriod = MilliSeconds(100);
    return vrnr::RunCttcNrOran(argc, argv, __FILE__, options);
}
