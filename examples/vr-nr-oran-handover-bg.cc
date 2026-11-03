// SPDX-License-Identifier: GPL-2.0-only

/**
 * @file vr-nr-oran-handover-bg.cc
 * @brief vr-nr-oran-handover with background VR traffic: one stationary UE next to each gNB.
 *
 * The scenario is described in vr-nr-oran-handover-scenario.h. Defaults: background UEs,
 * LOS channel, OFDMA round robin scheduler, TCP BBR for the TCP-based algorithms.
 * plot-vr-handover-bg.py plots the results of a run with and without --handover.
 */

#include "vr-nr-oran-handover-scenario.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    vrnr::HandoverOptions options;
    options.outPrefix = "vr-handover-bg";
    return vrnr::RunVrNrOranHandover(argc, argv, __FILE__, options);
}
