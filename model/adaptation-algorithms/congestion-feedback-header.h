/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CONGESTION_FEEDBACK_HEADER_H
#define CONGESTION_FEEDBACK_HEADER_H

#include "ns3/data-rate.h"
#include "ns3/header.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * @brief UDP feedback metrics reported by the media receiver.
 */
class CongestionFeedbackHeader : public Header
{
  public:
    /**
     * @brief Get the TypeId.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Get the instance TypeId.
     * @return The instance TypeId.
     */
    TypeId GetInstanceTypeId() const override;

    /**
     * @brief Get the serialized size.
     * @return The size in bytes.
     */
    uint32_t GetSerializedSize() const override;

    /**
     * @brief Serialize the congestion feedback.
     * @param start The buffer iterator to write to.
     */
    void Serialize(Buffer::Iterator start) const override;

    /**
     * @brief Deserialize the congestion feedback.
     * @param start The buffer iterator to read from.
     * @return The number of bytes consumed.
     */
    uint32_t Deserialize(Buffer::Iterator start) override;

    /**
     * @brief Print the congestion feedback.
     * @param os The output stream.
     */
    void Print(std::ostream& os) const override;

    /**
     * @brief Check the feedback signature and loss fraction.
     * @return True when the header contains valid feedback.
     */
    bool IsValid() const;

    /**
     * @brief Set receiver measurements.
     * @param queueDelay Estimated one-way queueing delay.
     * @param delayGradient Difference between arrival and departure intervals.
     * @param receiveRate Estimated receiver bitrate.
     * @param lossFraction Estimated packet loss fraction in [0, 1].
     */
    void SetMetrics(Time queueDelay, Time delayGradient, DataRate receiveRate, double lossFraction);

    /**
     * @brief Get the estimated one-way queueing delay.
     * @return The queueing delay.
     */
    Time GetQueueDelay() const;

    /**
     * @brief Get the measured delay gradient.
     * @return The delay gradient.
     */
    Time GetDelayGradient() const;

    /**
     * @brief Get the estimated receiver bitrate.
     * @return The receive bitrate.
     */
    DataRate GetReceiveRate() const;

    /**
     * @brief Get the estimated packet loss fraction.
     * @return The packet loss fraction in [0, 1].
     */
    double GetLossFraction() const;

  private:
    static constexpr uint32_t MAGIC = 0x4e533343; ///< Feedback datagram signature.
    uint32_t m_magic{MAGIC};                      ///< Serialized feedback signature.
    uint64_t m_queueDelayNs{0};                   ///< Queueing delay in nanoseconds.
    int64_t m_delayGradientNs{0};                 ///< Delay gradient in nanoseconds.
    uint64_t m_receiveRate{0};                    ///< Receive bitrate in bits per second.
    uint32_t m_lossFractionPpm{0};                ///< Loss fraction in parts per million.
};

} // namespace ns3

#endif // CONGESTION_FEEDBACK_HEADER_H
