#ifndef FUZZY_ALGORITHM_H
#define FUZZY_ALGORITHM_H

#include "ns3/data-rate.h"
#include "ns3/packet.h"

namespace ns3 {

class FuzzyAlgorithm
{
public:
  FuzzyAlgorithm ();
  virtual ~FuzzyAlgorithm ();

  DataRate fragmentReceived (const Ptr<Packet> &f);

private:
  DataRate fuzzyAlgorithm (Time delay, Time diffDelay, DataRate avgRate);

  Time m_lastFragmentTime;
  Time m_startedAt;
  std::multimap<Time, std::tuple<uint64_t, Time>> m_rateBuffer;

};

} // namespace ns3

#endif /* FUZZY_ALGORITHM_H */
