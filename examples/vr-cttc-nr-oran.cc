// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-cttc-nr-oran.cc
 * @brief VR over NR with O-RAN: UEs placed randomly around the gNB by GridScenarioHelper.
 *
 * The scenario is described in vr-cttc-nr-oran-scenario.h. The VR traffic uses a
 * GBR_CONV_VOICE QoS flow on the second bandwidth part.
 *
 * \code{.unparsed}
$ ./ns3 run "vr-cttc-nr-oran --PrintHelp"
    \endcode
 */

#include "vr-cttc-nr-oran-scenario.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VrCttcNrOran");

int
main(int argc, char* argv[])
{
    vrnr::CttcNrOranOptions options;
    options.vr.appRate = "50Mbps";
    return vrnr::RunCttcNrOran(argc, argv, __FILE__, options);
}
