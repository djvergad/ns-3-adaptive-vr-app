#ifndef ORAN_CELL_UTILIZATION_UDP_NO_QUEUE_ADAPTATION_ALGORITHM_H
#define ORAN_CELL_UTILIZATION_UDP_NO_QUEUE_ADAPTATION_ALGORITHM_H

#include "adaptation-algorithm-server.h"
#include "ns3/bursty-application-server-instance.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/oran-module.h"
#include <memory>

namespace ns3 {

class OranCellUtilizationUdpNoQueueAdaptationAlgorithm : public AdaptationAlgorithmServer
{
public:
  static TypeId GetTypeId (void);
  OranCellUtilizationUdpNoQueueAdaptationAlgorithm ();
  virtual ~OranCellUtilizationUdpNoQueueAdaptationAlgorithm ();

  // Set the collector instance
  // void SetCellUtilizationCollector (Ptr<OranCellUtilizationCollector> collector);
  Ptr<BurstyApplicationServerInstance> m_server_instance = nullptr;

private:
  // Override the adaptation algorithm
  virtual DataRate adaptation_algorithm (double buffOcc, double diffBuffOcc, DataRate lastRate);
  Ptr<OranLogicVrBitrate> m_lm;
  // Logical Channel ID for VR traffic bearer (configurable).
  uint8_t m_lcid = 5;
};

} // namespace ns3

#endif // ORAN_CELL_UTILIZATION_UDP_NO_QUEUE_ADAPTATION_ALGORITHM_H
