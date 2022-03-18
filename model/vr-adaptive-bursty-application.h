/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2021 SIGNET Lab, Department of Information Engineering,
// University of Padova
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef VR_ADAPTIVE_BURSTY_APPLICATION_H
#define VR_ADAPTIVE_BURSTY_APPLICATION_H

#include "bursty-application.h"
#include "vr-burst-generator.h"

namespace ns3 {

class Address;
class RandomVariableStream;
class Socket;
class BurstGenerator;

/**
 * \ingroup applications 
 * \defgroup bursty BurstyApplication
 *
 * This traffic generator supports large packets to be sent into smaller
 * packet (fragment) bursts.
 * The fragment size can be chosen arbitrarily, although it usually set
 * to match the MTU of the chosen network.
 * The burst size and period are controlled by a class extending the
 * BurstGenerator interface.
 * 
 */
/**
 * \ingroup bursty
 *
 * \brief Generate traffic to a single destination in bursty fashion.
 *
 * This traffic generator supports large packets to be sent into smaller
 * packet (fragment) bursts.
 * The fragment size can be chosen arbitrarily, although it usually set
 * to match the MTU of the chosen network.
 * The burst size and period are controlled by a class extending the
 * BurstGenerator interface.
 * 
 * This application assumes to operate on top of a UDP socket, sending
 * data to a BurstSink.
 * These two classes coexist, one fragmenting a large packet into a burst
 * of smaller fragments, the other by re-assembling the fragments into
 * the full packet.
 * 
 * Fragments have all the same length, which can be set via its attribute.
 * The last two segments of the burst might be shorter: the last one
 * because it represents the remainder of the burst size with respect to
 * the maximum frame size, the second to last because packets cannot be
 * smaller that the SeqTsSizeFragHeader size. If the last fragment is
 * too short, the second to last fragment is shortened in order to
 * increase the size of the last fragment.
 * Also, if a BurstGenerator generates a burst of size less than the
 * SeqTsSizeFragHeader size, the burst is discarded and a new burst is
 * queried to the generator.
 * 
 */
class VrAdaptiveBurstyApplication : public BurstyApplication
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  VrAdaptiveBurstyApplication ();

  virtual ~VrAdaptiveBurstyApplication ();

private:
  virtual void StartApplication (void); // Called at time specified by Start

  // Event handlers
  /**
   * \brief Sends a packet burst and schedules the next one
   */
  void HandleRead (Ptr<Socket>); // Called when a request is received

  DataRate initRate = DataRate(0);
};

} // namespace ns3

#endif /* VR_ADAPTIVE_BURSTY_APPLICATION_H */
