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
FuzzyAlgorithmServer::adaptation_algorithm (double buff_occ, double diff_buff_occ,
                                            DataRate lastRate)
{
  NS_LOG_FUNCTION (this);

  double empty = 0, ok = 0, full = 0, falling = 0, steady = 0, rising = 0, r1 = 0, r2 = 0, r3 = 0,
         r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0, p2 = 0, p1 = 0, z = 0, n1 = 0, n2 = 0,
         output = 0;

  double target_buff_occ = 2000; //bytes

  if (buff_occ == 0)
    {
      empty = 1.0;
    }
  else if (buff_occ < target_buff_occ)
    {
      empty = 1.0 - ((double) buff_occ) / target_buff_occ;
      ok = ((double) buff_occ) / target_buff_occ;
    }
  else if (buff_occ < 2 * target_buff_occ)
    {
      ok = 1 - ((double) buff_occ - target_buff_occ) / target_buff_occ;
      full = ((double) buff_occ - target_buff_occ) / target_buff_occ;
    }
  else
    {
      full = 1;
    }

  if (diff_buff_occ < -target_buff_occ)
    {
      falling = 1;
    }
  else if (diff_buff_occ < 0)
    {
      falling = -((double) diff_buff_occ) / target_buff_occ;
      steady = 1 + ((double) diff_buff_occ) / target_buff_occ;
    }
  else if (diff_buff_occ < target_buff_occ)
    {
      steady = 1 - ((double) diff_buff_occ) / target_buff_occ;
      rising = ((double) diff_buff_occ) / target_buff_occ;
    }
  else
    {
      rising = 1;
    }

  r9 = std::min (empty, falling);
  r8 = std::min (ok, falling);
  r7 = std::min (full, falling);
  r6 = std::min (empty, steady);
  r5 = std::min (ok, steady);
  r4 = std::min (full, steady);
  r3 = std::min (empty, rising);
  r2 = std::min (ok, rising);
  r1 = std::min (full, rising);

  p2 = std::sqrt (std::pow (r9, 2));
  p1 = std::sqrt (std::pow (r6, 2) + std::pow (r8, 2));
  z = std::sqrt (std::pow (r3, 2) + std::pow (r5, 2) + std::pow (r7, 2));
  n1 = std::sqrt (std::pow (r2, 2) + std::pow (r4, 2));
  n2 = std::sqrt (std::pow (r1, 2));

  /*output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 1.4 + p2 * 2)*/
  output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 2 + p2 * 4) / (n2 + n1 + z + p1 + p2);

  NS_LOG_DEBUG ("empty " << empty << " ok " << ok << " full " << full << " falling " << falling
                         << " steady " << steady << " rising " << rising << " output " << output);

  return DataRate (output * lastRate.GetBitRate ());
}

} // namespace ns3
