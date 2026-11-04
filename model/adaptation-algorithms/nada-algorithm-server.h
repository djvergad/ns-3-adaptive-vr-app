/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef NADA_ALGORITHM_SERVER_H
#define NADA_ALGORITHM_SERVER_H

#include "adaptation-algorithm-server.h"

namespace ns3
{

/**
 * @brief Application-layer NADA controller for UDP media.
 */
class NadaAlgorithmServer : public AdaptationAlgorithmServer
{
  public:
    /**
     * @brief Get the TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /** @brief Construct a NADA sender controller. */
    NadaAlgorithmServer();

    /** @brief Destroy the NADA sender controller. */
    ~NadaAlgorithmServer() override;

    /**
     * @brief Set the controller's starting and maximum bitrate.
     * @param rate The initial media bitrate.
     */
    void SetInitialRate(DataRate rate) override;

    /**
     * @brief Update the controller from a receiver feedback report.
     * @param queueDelay Estimated one-way queueing delay.
     * @param delayGradient Unused delay gradient.
     * @param receiveRate Estimated receiver bitrate.
     * @param lossFraction Estimated packet loss fraction in [0, 1].
     */
    void ReportFeedback(Time queueDelay,
                        Time delayGradient,
                        DataRate receiveRate,
                        double lossFraction) override;

  private:
    /**
     * @brief Return the current feedback-controlled bitrate.
     * @param buffOcc Unused socket buffer occupancy.
     * @param diffBuffOcc Unused buffer occupancy change.
     * @param lastRate Unused measured sender bitrate.
     * @return The NADA target bitrate.
     */
    DataRate adaptation_algorithm(double buffOcc, double diffBuffOcc, DataRate lastRate) override;

    DataRate m_startRate{DataRate("10Mbps")};     ///< Initial reference rate.
    DataRate m_initialRate{DataRate("100kbps")};  ///< Configured bitrate ceiling.
    DataRate m_minRate{DataRate("10kbps")};       ///< Minimum target bitrate.
    DataRate m_maxRate{DataRate("100Mbps")};      ///< Maximum target bitrate.
    Time m_queueDelayThreshold{MilliSeconds(50)}; ///< Delay-warping threshold.
    Time m_lossPenalty{MilliSeconds(50)};         ///< Loss penalty delay.
    Time m_referenceQueueDelay{MilliSeconds(10)}; ///< Equilibrium queueing delay.
    double m_referenceLossRate{0.02};             ///< Reference loss fraction.
    double m_priority{1.0};                       ///< Flow priority.
    double m_gain{0.5};                           ///< Congestion offset gain.
    double m_trendGain{2.0};                      ///< Congestion trend gain.
    double m_delayWarpFactor{1.0};                ///< Exponential warping factor.
    double m_increaseFactor{1.05};                ///< Accelerated ramp-up multiplier.
    double m_previousSignalSeconds{0.0};          ///< Previous congestion signal in seconds.
    Time m_lastFeedbackTime{Seconds(0)};          ///< Time of the previous report.
    bool m_hasPreviousSignal{false};              ///< Whether a previous signal exists.
};

} // namespace ns3

#endif // NADA_ALGORITHM_SERVER_H
