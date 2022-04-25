#include "google-algorithm-server.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GoogleAlgorithmServer");

NS_OBJECT_ENSURE_REGISTERED (GoogleAlgorithmServer);

TypeId
GoogleAlgorithmServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GoogleAlgorithmServer")
                          .SetParent<AdaptationAlgorithmServer> ()
                          .SetGroupName ("Applications")
                          .AddConstructor<GoogleAlgorithmServer> ();
  return tid;
}

GoogleAlgorithmServer::GoogleAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

GoogleAlgorithmServer::~GoogleAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

DataRate
GoogleAlgorithmServer::adaptation_algorithm (double buff_occ, double diff_buff_occ,
                                             DataRate lastRate)
{
  NS_LOG_FUNCTION (this);

  double aSlow = 0.99;
  double aFast = 0.98;

  m_Eslow = DataRate (aSlow * m_Eslow.GetBitRate () + (1 - aSlow) * lastRate.GetBitRate ());
  m_Efast = DataRate (aFast * m_Efast.GetBitRate () + (1 - aFast) * lastRate.GetBitRate ());

  return DataRate (0.95 * std::min (m_Eslow, m_Efast).GetBitRate ());
}

} // namespace ns3
