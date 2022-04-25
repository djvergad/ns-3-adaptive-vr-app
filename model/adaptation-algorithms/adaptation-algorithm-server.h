#ifndef ADAPTATION_ALGORITHM_SERVER_H
#define ADAPTATION_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

namespace ns3 {

class AdaptationAlgorithmServer : public Object
{
public:
  static TypeId GetTypeId (void);

  AdaptationAlgorithmServer ();
  virtual ~AdaptationAlgorithmServer ();

  virtual DataRate nextBurstRate (Ptr<TcpSocketBase> socket, uint64_t bytesAddedToSocket,
                                  Time txTime);

protected:
  virtual DataRate adaptation_algorithm (double buff_occ, double diff_buff_occ,
                                         DataRate lastRate) = 0;

  Time m_lastBurstTime = Seconds (0);

  uint64_t m_lastBufferOcc = 0;
};

} // namespace ns3

#endif /* ADAPTATION_ALGORITHM_SERVER_H */
