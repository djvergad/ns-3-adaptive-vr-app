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
 */

#ifndef VR_ADAPTIVE_BURST_SINK_H
#define VR_ADAPTIVE_BURST_SINK_H

#include "burst-sink.h"

namespace ns3 {

class Address;
class Socket;
class Packet;

/**
 * \ingroup applications 
 * \defgroup burstsink BurstSink
 *
 * This application was written to complement BurstyApplication.
 */

/**
 * \ingroup burstsink
 * 
 * \brief Receives and consume traffic generated from a BurstyApplication
 * to an IP address and port
 *
 * This application was based on PacketSink, and modified to accept traffic
 * generated by BurstyApplication.
 * The application assumes to be on top of a UDP socket, thus receiving UDP
 * datagrams rather than bytestreams.
 *
 * The sink tries to aggregate single packets (fragments) into a single packet
 * burst. To do so, it gathers information from SeqTsSizeFragHeader, which all
 * received packets should have.
 * It then makes the following assumptions:
 * - Being based on a UDP socket, packets might arrive out-of-order. Within a
 * burst, BurstSink will reorder the received packets.
 * - While receiving burst n, if a fragment from burst k<n is received, the
 * fragment is discarded
 * - While receiving burst n, if a fragment from burst k>n is received,
 * burst n is discarded and burst k will start being buffered.
 * - If all fragments from a burst are received, the burst is successfully
 * received.
 * 
 * Traces are sent when a fragment is received and when a whole burst is
 * successfully received.
 * 
 */
class VrAdaptiveBurstSink : public BurstSink
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  VrAdaptiveBurstSink ();
  virtual ~VrAdaptiveBurstSink ();

private:
  /**
   * \brief Fragment received: assemble byte stream to extract SeqTsSizeFragHeader
   * \param f received fragment
   * \param from from address
   * \param localAddress local address
   *
   * The method assembles a received byte stream and extracts SeqTsSizeFragHeader
   * instances from the stream to export in a trace source.
   */
  void FragmentReceived (BurstHandler &burstHandler, const Ptr<Packet> &f, const Address &from,
                         const Address &localAddress);

  std::map<Address, Time> m_lastFragmentTimes;
  std::map<Address, Time> m_started_ats;
  std::map<Address, std::multimap<Time, std::tuple<uint64_t, Time>>> m_rateBuffers;
};

} // namespace ns3

#endif /* VR_ADAPTIVE_BURST_SINK_H */
