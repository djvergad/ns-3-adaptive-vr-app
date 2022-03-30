#ifndef FUZZY_ALGORITHM_SERVER_H
#define FUZZY_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/packet.h"

namespace ns3 {

class FuzzyAlgorithmServer
{
public:
  FuzzyAlgorithmServer ();
  virtual ~FuzzyAlgorithmServer ();

  DataRate nextBurstRate (Ptr<TcpSocketBase> socket, uint64_t bytesAddedToSocket);

private:
  DataRate fuzzyAlgorithm (double buff_occ, double diff_buff_occ, DataRate lastRate);

  Time m_lastBurstTime = Seconds (0);
  uint64_t m_lastBufferOcc = 0;
};

} // namespace ns3

#endif /* FUZZY_ALGORITHM_SERVER_H */
