#ifndef FUZZY_ALGORITHM_SERVER_H
#define FUZZY_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/packet.h"
#include "adaptation-algorithm-server.h"

namespace ns3 {

class FuzzyAlgorithmServer : public AdaptationAlgorithmServer
{
public:
  static TypeId GetTypeId (void);

  FuzzyAlgorithmServer ();
  virtual ~FuzzyAlgorithmServer ();

private:
  DataRate adaptation_algorithm (double buffOcc, double diffBuffOcc, DataRate lastRate);

  uint32_t m_lowRateStreak = 0;
  uint32_t m_probeCooldown = 0;
};

} // namespace ns3

#endif /* FUZZY_ALGORITHM_SERVER_H */
