#ifndef ORAN_CELL_UTILIZATION_UDP_ADAPTATION_ALGORITHM_H
#define ORAN_CELL_UTILIZATION_UDP_ADAPTATION_ALGORITHM_H

#include "adaptation-algorithm-server.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include <memory>

namespace ns3 {

class OranCellUtilizationCollector; // Forward declaration

class OranCellUtilizationUdpAdaptationAlgorithm : public AdaptationAlgorithmServer
{
public:
  static TypeId GetTypeId (void);
  OranCellUtilizationUdpAdaptationAlgorithm ();
  virtual ~OranCellUtilizationUdpAdaptationAlgorithm ();

private:
  // Override the adaptation algorithm
  virtual DataRate adaptation_algorithm (double buffOcc, double diffBuffOcc, DataRate lastRate);
  double m_utilizationThreshold = 100000;
  DataRate m_minRate = DataRate ("1.5Mbps");
  DataRate m_maxRate = DataRate ("50Mbps");
  DataRate m_prevRate = DataRate ("1.5Mbps");
  Time m_lastAdaptationTimeUp = Seconds (0.2);
  Time m_lastAdaptationTimeDown = Seconds (0.2);
  // Smoothing (exponential moving average) for utilization to avoid reacting to spikes
  double m_utilizationEma = -1.0;
  double m_txQueueSizeEma = -1.0;
  double m_emaAlpha = 0.6; // higher = react faster, lower = smoother
  // Hysteresis (percentage points) around threshold to avoid ping-pong
  double m_hysteresisPercent = 10; // percent points
  // AIMD parameters (Additive Increase, Multiplicative Decrease)
  DataRate m_additiveIncrease = DataRate ("0.4Mbps");
  double m_multiplicativeDecreaseFactor = 0.2; // multiply rate by this factor on decrease

  // Slow start parameters (similar to TCP slow start)
  bool m_slowStartEnabled = true;
  bool m_inSlowStart = true;
  DataRate m_ssthresh = DataRate ("20Mbps");
  double m_slowStartMultiplier = 2.0; // double the rate each step while in slow start

  // Minimum time between adaptations for up/down to avoid too frequent tiny changes
  Time m_minAdaptInterval = MilliSeconds (30);
  // Random jitter for multiplicative decrease (percent)
  Ptr<UniformRandomVariable> m_random = CreateObject<UniformRandomVariable> ();
  double m_decreaseJitterPercent = 10.0; // +/- percent around multiplicative factor

  // Logical Channel ID for VR traffic (configurable to match actual bearer LCID)
  uint8_t m_lcid = 5; // Default to 5, but can be configured from script

  // Selects Predictive-BRAM derivative-aware bitrate query.
  bool m_useDerivativeBitrate = false;
  bool m_useOptimizedBitrate = false;
  bool m_useDqnBitrate = false;
};

} // namespace ns3

#endif // ORAN_CELL_UTILIZATION_UDP_ADAPTATION_ALGORITHM_H
