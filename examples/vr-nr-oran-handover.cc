// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-nr-oran-handover.cc
 * @brief Downlink VR traffic to a moving UE that is handed over between NR gNBs by an O-RAN xApp.
 *
 * The scenario is described in vr-nr-oran-handover-scenario.h; this program runs it with the
 * moving UE only (no background UEs). plot-vr-handover.py plots the results of a run with and
 * without --handover.
 */

#include "vr-nr-oran-handover-scenario.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    vrnr::HandoverOptions options;
    options.backgroundUes = false;
    options.outPrefix = "vr-handover";
    return vrnr::RunVrNrOranHandover(argc, argv, __FILE__, options);
}
