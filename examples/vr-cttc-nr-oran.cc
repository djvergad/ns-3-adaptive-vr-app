// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-cttc-nr-oran.cc
 * @brief VR over NR with O-RAN: UEs placed randomly in a 3 x 3 m area next to the gNB.
 *
 * The scenario is described in vr-cttc-nr-oran-scenario.h. UMi channel without shadowing,
 * so that all the UEs have a similarly strong channel.
 *
 * \code{.unparsed}
$ ./ns3 run "vr-cttc-nr-oran --PrintHelp"
    \endcode
 */

#include "vr-cttc-nr-oran-scenario.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    vrnr::CttcNrOranOptions options;
    return vrnr::RunCttcNrOran(argc, argv, __FILE__, options);
}
