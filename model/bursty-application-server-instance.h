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

#ifndef BURSTY_APPLICATION_SERVER_INSTANCE_H
#define BURSTY_APPLICATION_SERVER_INSTANCE_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"
#include "ns3/seq-ts-size-frag-header.h"
#include "vr-burst-generator.h"

#include <queue>

namespace ns3 {

class Address;
class RandomVariableStream;
class Socket;
class BurstGenerator;

class BurstyApplicationServerInstance : public Object
{
public:
  static TypeId GetTypeId (void);

  BurstyApplicationServerInstance ();

  // BurstyApplicationServerInstance (Ptr<Socket> socket, Address peer,
  //                                  TracedCallback<Ptr<const Packet>, const Address &,
  //                                                 const Address &, const SeqTsSizeFragHeader &>
  //                                      m_txBurstTrace,
  //                                  TracedCallback<Ptr<const Packet>, const Address &,
  //                                                 const Address &, const SeqTsSizeFragHeader &>
  //                                      m_txFragmentTrace);

  virtual ~BurstyApplicationServerInstance ();

  /**
   * \brief Return a pointer to associated socket.
   * \return pointer to associated socket
   */
  Ptr<Socket> GetSocket (void) const;

  /**
   * \brief Returns a pointer to the associated BurstGenerator
   * \return pointer to associated BurstGenerator
   */
  Ptr<BurstGenerator> GetBurstGenerator () const;

  /**
   * \brief Return the total number of transmitted bursts.
   * \return number of transmitted bursts
   */
  uint64_t GetTotalTxBursts () const;

  /**
   * \brief Return the total number of transmitted fragments.
   * \return number of transmitted fragments
   */
  uint64_t GetTotalTxFragments () const;

  /**
   * \brief Return the total number of transmitted bytes.
   * \return number of transmitted bytes
   */
  uint64_t GetTotalTxBytes (void) const;

  void SetIsAdaptive (bool value);
  bool GetIsAdaptive (void) const;


  friend class BurstyApplicationServer;

protected:
  virtual void DoDispose (void);

  virtual void StopApplication (void); // Called at time specified by Stop

  //helpers
  /**
   * \brief Cancel all pending events.
   */
  void CancelEvents ();

  // Event handlers
  /**
   * \brief Sends a packet burst and schedules the next one
   */
  virtual void SendBurst ();

  /**
   * \brief Send burst fragmented into multiple packets
   * \param burstSize the size of the burst in Bytes
   */
  void SendFragmentedBurst (uint32_t burstSize);

  /**
   * \brief Send a single fragment
   * \param fragment the fragment to send
   * \param burstSize size of the entire burst in bytes
   * \param totFrags the number of fragments composing the burst
   * \param fragmentSeq the sequence number of the fragment
   */
  void SendFragment (Ptr<Packet> fragment, uint64_t burstSize, uint16_t totFrags,
                     uint16_t fragmentSeq);

  Ptr<Socket> m_socket; //!< Associated socket
  Address m_peer; //!< Peer address
  Address m_local; //!< Local address to bind to
  bool m_connected; //!< True if connected
  Ptr<BurstGenerator> m_burstGenerator =
      CreateObject<VrBurstGenerator> (); //!< Burst generator class
  uint32_t m_fragSize = 1200; //!< Size of fragments including SeqTsSizeFragHeader
  EventId m_nextBurstEvent; //!< Event id for the next packet burst
  TypeId m_socketTid; //!< Type of the socket used
  uint64_t m_totTxBursts; //!< Total bursts sent
  uint64_t m_totTxFragments; //!< Total fragments sent
  uint64_t m_totTxBytes; //!< Total bytes sent

  // Traced Callbacks
  /// Callback for transmitted burst
  TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader &>
      m_txBurstTrace;
  /// Callback for transmitted fragment
  TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader &>
      m_txFragmentTrace;

  void DataSend (Ptr<Socket>, uint32_t); // Called when a new segment is transmitted
  // A structure that contains the generated MPEG frames, for each client.
  std::deque<Packet> m_queue;
  uint32_t m_queueSize = 1000;

  DataRate m_initRate = 0;
  Time m_lastBurstAt = Seconds (0);

  void AdaptRate();

  bool m_isAdaptive = false;
};

} // namespace ns3

#endif /* BURSTY_APPLICATION_SERVER_INSTANCE_H */
