/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GCC_ALGORITHM_SERVER_H
#define GCC_ALGORITHM_SERVER_H

#include "adaptation-algorithm-server.h"

namespace ns3
{

/**
 * @brief Application-layer GCC controller for UDP media.
 */
class GccAlgorithmServer : public AdaptationAlgorithmServer
{
  public:
    /**
     * @brief Get the TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /** @brief Construct a GCC sender controller. */
    GccAlgorithmServer();

    /** @brief Destroy the GCC sender controller. */
    ~GccAlgorithmServer() override;

    /**
     * @brief Set the controller's starting and maximum bitrate.
     * @param rate The initial media bitrate.
     */
    void SetInitialRate(DataRate rate) override;

    /**
     * @brief Update the controller from a receiver feedback report.
     * @param queueDelay Estimated one-way queueing delay.
     * @param delayGradient Difference between arrival and departure intervals.
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
     * @return The GCC target bitrate.
     */
    DataRate adaptation_algorithm(double buffOcc, double diffBuffOcc, DataRate lastRate) override;

    DataRate m_delayRate{DataRate("100kbps")};    ///< Delay-controller bitrate.
    DataRate m_lossRate{DataRate("100kbps")};     ///< Loss-controller bitrate.
    DataRate m_startRate{DataRate("10Mbps")};     ///< Initial target rate.
    bool m_holdDelayRate{false};                  ///< Hold the delay rate after a decrease.
    DataRate m_initialRate{DataRate("100kbps")};  ///< Configured bitrate ceiling.
    DataRate m_minRate{DataRate("10kbps")};       ///< Minimum target bitrate.
    DataRate m_maxRate{DataRate("100Mbps")};      ///< Maximum target bitrate.
    Time m_overuseThreshold{MilliSeconds(12.5)};  ///< Starting overuse threshold.
    double m_increaseFactor{1.05};                ///< Rate increase factor.
    double m_decreaseFactor{0.85};                ///< Overuse rate factor.
    double m_lossThreshold{0.02};                 ///< Loss threshold for rate increase.
    double m_highLossThreshold{0.1};              ///< Loss threshold for rate decrease.
    double m_lossDecreaseFactor{0.5};             ///< Loss-dependent decrease coefficient.
    double m_filteredGradientSeconds{0.0};        ///< Smoothed delay gradient in seconds.
    Time m_adaptiveThreshold{MilliSeconds(12.5)}; ///< Current overuse threshold.
    bool m_hasGradient{false};                    ///< Whether a gradient sample was received.
};

} // namespace ns3

#endif // GCC_ALGORITHM_SERVER_H
