#ifndef GOOGLE_ALGORITHM_SERVER_H
#define GOOGLE_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/packet.h"
#include "adaptation-algorithm-server.h"

namespace ns3 {

class GoogleAlgorithmServer : public AdaptationAlgorithmServer
{
public:
  static TypeId GetTypeId (void);

  GoogleAlgorithmServer ();
  virtual ~GoogleAlgorithmServer ();

private:
  DataRate adaptation_algorithm (double buff_occ, double diff_buff_occ, DataRate lastRate);

  DataRate m_Eslow = DataRate ("100kbps");
  DataRate m_Efast = DataRate ("100kbps");
};

} // namespace ns3

#endif /* GOOGLE_ALGORITHM_SERVER_H */
