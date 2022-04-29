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

  double target_buff_occ = 2000; //bytes

  if (buffOcc == 0)
    {
      empty = 1.0;
    }
  else if (buffOcc < target_buff_occ)
    {
      empty = 1.0 - ((double) buffOcc) / target_buff_occ;
      ok = ((double) buffOcc) / target_buff_occ;
    }
  else if (buffOcc < 2 * target_buff_occ)
    {
      ok = 1 - ((double) buffOcc - target_buff_occ) / target_buff_occ;
      full = ((double) buffOcc - target_buff_occ) / target_buff_occ;
    }
  else
    {
      full = 1;
    }

  if (diffBuffOcc < -target_buff_occ)
    {
      falling = 1;
    }
  else if (diffBuffOcc < 0)
    {
      falling = -((double) diffBuffOcc) / target_buff_occ;
      steady = 1 + ((double) diffBuffOcc) / target_buff_occ;
    }
  else if (diffBuffOcc < target_buff_occ)
    {
      steady = 1 - ((double) diffBuffOcc) / target_buff_occ;
      rising = ((double) diffBuffOcc) / target_buff_occ;
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

  return DataRate (output * lastRate.GetBitRate ());
}

} // namespace ns3
