#include "fuzzy-algorithm-server.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FuzzyAlgorithmServer");

NS_OBJECT_ENSURE_REGISTERED (FuzzyAlgorithmServer);

TypeId
FuzzyAlgorithmServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FuzzyAlgorithmServer")
                          .SetParent<AdaptationAlgorithmServer> ()
                          .SetGroupName ("Applications")
                          .AddConstructor<FuzzyAlgorithmServer> ();
  return tid;
}

FuzzyAlgorithmServer::FuzzyAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

FuzzyAlgorithmServer::~FuzzyAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

DataRate
FuzzyAlgorithmServer::adaptation_algorithm (double buffOcc, double diffBuffOcc, DataRate lastRate)
{
  NS_LOG_FUNCTION (this);

  double empty = 0, ok = 0, full = 0, falling = 0, steady = 0, rising = 0;

  double targetBuffOcc = 2000; //bytes

  if (buffOcc == 0)
    {
      empty = 1.0;
    }
  else if (buffOcc < targetBuffOcc)
    {
      empty = 1.0 - ((double) buffOcc) / targetBuffOcc;
      ok = ((double) buffOcc) / targetBuffOcc;
    }
  else if (buffOcc < 2 * targetBuffOcc)
    {
      ok = 1 - ((double) buffOcc - targetBuffOcc) / targetBuffOcc;
      full = ((double) buffOcc - targetBuffOcc) / targetBuffOcc;
    }
  else
    {
      full = 1;
    }

  if (diffBuffOcc < -targetBuffOcc)
    {
      falling = 1;
    }
  else if (diffBuffOcc < 0)
    {
      falling = -((double) diffBuffOcc) / targetBuffOcc;
      steady = 1 + ((double) diffBuffOcc) / targetBuffOcc;
    }
  else if (diffBuffOcc < targetBuffOcc)
    {
      steady = 1 - ((double) diffBuffOcc) / targetBuffOcc;
      rising = ((double) diffBuffOcc) / targetBuffOcc;
    }
  else
    {
      rising = 1;
    }

  double r1 = std::min (full, rising);
  double r2 = std::min (ok, rising);
  double r3 = std::min (empty, rising);
  double r4 = std::min (full, steady);
  double r5 = std::min (ok, steady);
  double r6 = std::min (empty, steady);
  double r7 = std::min (full, falling);
  double r8 = std::min (ok, falling);
  double r9 = std::min (empty, falling);

  double R = std::sqrt (std::pow (r1, 2));
  double SR = std::sqrt (std::pow (r2, 2) + std::pow (r4, 2));
  double NC = std::sqrt (std::pow (r3, 2) + std::pow (r5, 2) + std::pow (r7, 2));
  double SI = std::sqrt (std::pow (r6, 2) + std::pow (r8, 2));
  double I = std::sqrt (std::pow (r9, 2));

  double N2 = 0.25, N1 = 0.5, Z = 1, P1 = 2, P2 = 4;

  double output = (R * N2 + SR * N1 + NC * Z + SI * P1 + I * P2) / (R + SR + NC + SI + I);

  NS_LOG_DEBUG ("empty " << empty << " ok " << ok << " full " << full << " falling " << falling
                         << " steady " << steady << " rising " << rising << " output " << output);

  DataRate result_non_quant = DataRate(output * lastRate.GetBitRate ());

  std::vector<DataRate> averageBitrate = { 3128000, 3254000, 3974000, 4496000, 6408000, 10938000, 17156000, 35018000 };

  for (uint32_t i = 1; i < averageBitrate.size(); i++) {
    if (averageBitrate[i] > result_non_quant) {
      return averageBitrate[i - 1];
    }
  }

  return averageBitrate[averageBitrate.size() - 1];
}

} // namespace ns3
