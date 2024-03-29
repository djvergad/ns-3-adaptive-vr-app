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

#ifndef BURSTY_APPLICATION_SERVER_H
#define BURSTY_APPLICATION_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/seq-ts-size-frag-header.h"
#include "bursty-application-server-instance.h"
#include <unordered_map>

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
class BurstyApplicationServer : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  BurstyApplicationServer ();
  virtual ~BurstyApplicationServer ();

  /**
   * \return the total bytes received in this sink app
   */
  uint64_t GetTotalTxBytes () const;

  /**
   * \return the total fragments received in this sink app
   */
  uint64_t GetTotalTxFragments () const;

  /**
   * \return the total bursts received in this sink app
   */
  uint64_t GetTotalTxBursts () const;

  /**
   * \return pointer to listening socket
   */
  Ptr<Socket> GetListeningSocket (void) const;

  /**
   * \return list of pointers to accepted sockets
   */
  std::list<Ptr<Socket>> GetAcceptedSockets (void) const;

  std::map<Address, BurstyApplicationServerInstance> GetInstances (void) const;

protected:
  virtual void DoDispose (void);

  // private:
  // inherited from Application base class.
  virtual void StartApplication (void); // Called at time specified by Start
  virtual void StopApplication (void); // Called at time specified by Stop

  /**
   * \brief Handle a fragment received by the application
   * \param socket the receiving socket
   */
  virtual void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Handle an incoming connection
   * \param socket the incoming connection socket
   * \param from the address the connection is from
   */
  void HandleAccept (Ptr<Socket> socket, const Address &from);
  /**
   * \brief Handle a connection close
   * \param socket the connected socket
   */
  void HandlePeerClose (Ptr<Socket> socket);
  /**
   * \brief Handle a connection error
   * \param socket the connected socket
   */
  void HandlePeerError (Ptr<Socket> socket);
  /**
   * \brief Handle a connection close
   * \param socket the connected socket
   */
  void HandleClose (Ptr<Socket> socket);
  /**
   * \brief Handle a connection error
   * \param socket the connected socket
   */
  void HandleError (Ptr<Socket> socket);

  // In the case of TCP, each socket accept returns a new socket, so the
  // listening socket is stored separately from the accepted sockets
  Ptr<Socket> m_socket{0}; //!< Listening socket
  std::list<Ptr<Socket>> m_socketList; //!< the accepted sockets
  Address m_local; //!< Local address to bind to
  TypeId m_tid; //!< Protocol TypeId

  std::map<Address, BurstyApplicationServerInstance> m_server_instances;

  DataRate m_maxTargetDataRate;

  // Traced Callbacks
  /// Callback for transmitted burst
  TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader &>
      m_txBurstTrace;
  /// Callback for transmitted fragment
  TracedCallback<Ptr<const Packet>, const Address &, const Address &, const SeqTsSizeFragHeader &>
      m_txFragmentTrace;
  void DataSend (Ptr<Socket>, uint32_t); // Called when a new segment is transmitted

  void CreateInstance (Ptr<Socket> socket, Address peer);

  std::string m_adaptationAlgorithm = "";
  uint32_t m_fragSize = 1200; //!< Size of fragments including SeqTsSizeFragHeader

  Time m_appDuration = Seconds (1);
};

} // namespace ns3

#endif /* BURSTY_APPLICATION_SERVER_H */
