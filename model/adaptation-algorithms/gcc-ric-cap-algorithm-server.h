/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GCC_RIC_CAP_ALGORITHM_SERVER_H
#define GCC_RIC_CAP_ALGORITHM_SERVER_H

#include "gcc-algorithm-server.h"

#include "ns3/address.h"
#include "ns3/bursty-application-server-instance.h"
#include "ns3/oran-vr-bitrate-service.h"

namespace ns3
{

/**
 * @brief GCC with its rate capped by the bitrate of the Near-RT RIC.
 *
 * The end-to-end GCC controller runs unchanged, and its target rate is limited to the latest
 * bitrate received from an OranVrBitrateServer, as when a network-assisted cap is placed over
 * an over-the-top controller. The cap (CapSource) is the fair share of the cell, the fair share
 * with the queue back-off of the RIC, or the NetXRate recommendation, alone or with the queue
 * back-off. The internal rates of GCC
 * are limited to the cap as well, so
 * that they do not wind up above it while the cap is binding. Until the first answer from the
 * RIC arrives, the controller behaves as plain GCC.
 */
class GccRicCapAlgorithmServer : public GccAlgorithmServer
{
  public:
    /**
     * @brief Get the TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    GccRicCapAlgorithmServer();
    ~GccRicCapAlgorithmServer() override;

    /**
     * @brief Update GCC from a receiver feedback report and apply the cap.
     * @param queueDelay Estimated one-way queueing delay.
     * @param delayGradient Difference between arrival and departure intervals.
     * @param receiveRate Estimated receiver bitrate.
     * @param lossFraction Estimated packet loss fraction in [0, 1].
     */
    void ReportFeedback(Time queueDelay,
                        Time delayGradient,
                        DataRate receiveRate,
                        double lossFraction) override;

    /**
     * @brief Get the latest cap received from the RIC.
     * @return The cap, or DataRate(0) if none has been received.
     */
    DataRate GetCap() const;

    Ptr<BurstyApplicationServerInstance> m_server_instance = nullptr; ///< Server instance

  protected:
    void DoDispose() override;

  private:
    /**
     * @brief Ask the RIC for a fresh cap and return the capped GCC rate.
     * @param buffOcc Unused socket buffer occupancy.
     * @param diffBuffOcc Unused buffer occupancy change.
     * @param lastRate Unused measured sender bitrate.
     * @return The capped GCC target bitrate.
     */
    DataRate adaptation_algorithm(double buffOcc, double diffBuffOcc, DataRate lastRate) override;

    /** @brief Limit the GCC rates to the latest cap, if there is one. */
    void ApplyCap();

    Address m_ricServer;                ///< Address of the OranVrBitrateServer
    uint8_t m_lcid{5};                  ///< Logical channel of the VR traffic
    OranVrBitrateServer::RateMode m_capSource{OranVrBitrateServer::NO_QUEUE}; ///< Cap source
    Ptr<OranVrBitrateClient> m_ricClient; ///< Client for the RIC bitrate service
};

} // namespace ns3

#endif // GCC_RIC_CAP_ALGORITHM_SERVER_H
