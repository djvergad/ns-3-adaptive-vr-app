/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NETXRATE_UDP_ADAPTATION_ALGORITHM_H
#define NETXRATE_UDP_ADAPTATION_ALGORITHM_H

#include "adaptation-algorithm-server.h"

#include "ns3/bursty-application-server-instance.h"
#include "ns3/oran-logic-vr-bitrate.h"

namespace ns3
{

/**
 * @brief Application-side NetXRate cap using shared O-RAN cell measurements.
 */
class NetXRateUdpAdaptationAlgorithm : public AdaptationAlgorithmServer
{
  public:
    /**
     * Get the TypeId.
     *
     * @return The type ID.
     */
    static TypeId GetTypeId();

    /** Construct a NetXRate application controller. */
    NetXRateUdpAdaptationAlgorithm();

    /** Destroy the NetXRate application controller. */
    ~NetXRateUdpAdaptationAlgorithm() override;

    /**
     * Set the required application rate used as NetXRate's per-UE demand.
     *
     * @param rate The configured application rate.
     */
    void SetInitialRate(DataRate rate) override;

    BurstyApplicationServerInstance* m_server_instance{nullptr}; ///< Owning UE stream.

  private:
    /**
     * Return the most recently calculated NetXRate recommendation.
     *
     * @param buffOcc Unused socket buffer occupancy.
     * @param diffBuffOcc Unused buffer occupancy change.
     * @param lastRate Unused measured sender rate.
     * @return The recommendation, capped at the configured application rate.
     */
    DataRate adaptation_algorithm(double buffOcc, double diffBuffOcc, DataRate lastRate) override;

    Ptr<OranLogicVrBitrate> m_lm; ///< Shared cell statistics and NetXRate allocator.
    uint8_t m_lcid{5};            ///< Logical channel carrying VR traffic.
    Time m_recommendationPeriod{MilliSeconds(500)};       ///< NetXRate report period.
    Time m_lastRecommendationTime{Seconds(0)};            ///< Time the cached cap was calculated.
    DataRate m_initialRate{DataRate("100kbps")};          ///< Configured application-rate demand.
    DataRate m_cachedRecommendation{DataRate("100kbps")}; ///< Last reported cell allocation.
    bool m_hasRecommendation{false}; ///< Whether a recommendation has been calculated.
    bool m_queueAware{false};        ///< Apply the queue back-off to the recommendation.
};

} // namespace ns3

#endif // NETXRATE_UDP_ADAPTATION_ALGORITHM_H
