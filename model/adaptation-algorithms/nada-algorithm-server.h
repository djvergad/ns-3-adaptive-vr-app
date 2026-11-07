/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef NADA_ALGORITHM_SERVER_H
#define NADA_ALGORITHM_SERVER_H

#include "adaptation-algorithm-server.h"

#include <deque>

namespace ns3
{

/**
 * @brief Application-layer NADA controller for UDP media, following RFC 8698.
 *
 * The receiver-side logic of the RFC runs here at the sender, on the raw measurements of the
 * feedback report. The differences from the RFC are:
 * - the report carries a loss fraction and no loss count, so a loss event is inferred from a rise
 *   of that fraction, and the loss interval is measured in time rather than in packets;
 * - the loss fraction is the client's per-burst average, not Eq. 10 of the RFC;
 * - the queueing delay is the latest sample of each report, without the RFC's minimum filter, so
 *   the ramp-up condition sees one delay sample per report;
 * - the round-trip time is configured, because the report carries no timestamp echo;
 * - ECN marking is not modelled, so the marking term of Eq. 2 is zero.
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

    /**
     * @brief Record a loss event if the reported loss fraction rose since the last report.
     * @param now The current time.
     */
    void UpdateLossHistory(Time now);

    /**
     * @brief Get the average loss interval (loss_int), weighted as in RFC 5348, Section 5.4.
     *
     * Only closed intervals are used. Including the open interval, as TFRC does, would stretch
     * the expiration window with the age of the last loss, so that it could never expire.
     *
     * @return The average interval between loss events.
     */
    Time GetAverageLossInterval() const;

    /**
     * @brief Apply the delay warping of RFC 8698, Eq. 1, while a recent loss is in effect.
     *
     * The warped value holds until the last loss_int of the loss_exp window, and is then
     * interpolated linearly back to the raw delay, as the RFC recommends.
     *
     * @param now The current time.
     * @param delaySeconds The queueing delay in seconds.
     * @return The delay d_tilde in seconds.
     */
    double WarpDelay(Time now, double delaySeconds) const;

    /**
     * @brief Check the conditions of accelerated ramp-up (rmode = 0).
     * @param now The current time.
     * @return Whether neither loss nor delay buildup occurred within LOGWIN.
     */
    bool CanRampUp(Time now) const;

    DataRate m_startRate{DataRate("10Mbps")};      ///< Initial reference rate.
    DataRate m_initialRate{DataRate("100kbps")};   ///< Configured bitrate ceiling (RMAX).
    DataRate m_minRate{DataRate("10kbps")};        ///< Minimum target bitrate (RMIN).
    DataRate m_maxRate{DataRate("100Mbps")};       ///< Maximum target bitrate.
    Time m_queueDelayThreshold{MilliSeconds(50)};  ///< Delay-warping threshold (QTH).
    Time m_lossPenalty{MilliSeconds(10)};          ///< Loss penalty delay (DLOSS).
    Time m_referenceQueueDelay{MilliSeconds(10)};  ///< Reference congestion signal (XREF).
    Time m_timeConstant{MilliSeconds(500)};        ///< Rate-update time constant (TAU).
    Time m_feedbackInterval{MilliSeconds(100)};    ///< Target feedback interval (DELTA).
    Time m_delayFilterDelay{MilliSeconds(120)};    ///< Delay-filter latency (DFILT).
    Time m_rampDelayBound{MilliSeconds(50)};       ///< Self-inflicted delay bound (QBOUND).
    Time m_rampDelayThreshold{MilliSeconds(10)};   ///< Delay buildup threshold (QEPS).
    Time m_observationWindow{MilliSeconds(500)};   ///< Ramp-up observation window (LOGWIN).
    Time m_roundTripTime{MilliSeconds(20)};        ///< Configured round-trip time (rtt).
    double m_referenceLossRate{0.01};              ///< Reference loss fraction (PLRREF).
    double m_priority{1.0};                        ///< Flow priority (PRIO).
    double m_gain{0.5};                            ///< Congestion offset gain (KAPPA).
    double m_trendGain{2.0};                       ///< Congestion trend gain (ETA).
    double m_delayWarpFactor{0.5};                 ///< Exponential warping factor (LAMBDA).
    double m_gammaMax{0.5};                        ///< Maximum ramp-up multiplier (GAMMA_MAX).
    double m_lossExpiryMultiplier{7.0};            ///< Loss window multiplier (MULTILOSS).
    double m_previousSignalSeconds{0.0};           ///< Previous congestion signal (x_prev).
    double m_previousLossFraction{0.0};            ///< Loss fraction of the previous report.
    Time m_firstFeedbackTime{Seconds(0)};          ///< Time of the first report.
    Time m_lastFeedbackTime{Seconds(0)};           ///< Time of the previous report (t_last).
    Time m_lastLossTime{Seconds(0)};               ///< Time of the last loss event.
    Time m_lastDelayBuildupTime{Seconds(0)};       ///< Last time the delay reached QEPS.
    bool m_hasFeedback{false};                     ///< Whether a report was received.
    bool m_hasLoss{false};                         ///< Whether a loss event occurred.
    bool m_hasDelayBuildup{false};                 ///< Whether the delay ever reached QEPS.
    std::deque<Time> m_lossIntervals;              ///< Closed loss intervals, newest first.
};

} // namespace ns3

#endif // NADA_ALGORITHM_SERVER_H
