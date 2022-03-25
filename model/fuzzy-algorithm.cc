#include "fuzzy-algorithm.h"
#include "ns3/log.h"
#include "seq-ts-size-frag-header.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FuzzyAlgorithm");

FuzzyAlgorithm::FuzzyAlgorithm ()
{
  NS_LOG_FUNCTION (this);
}

FuzzyAlgorithm::~FuzzyAlgorithm ()
{
  NS_LOG_FUNCTION (this);
}

DataRate
FuzzyAlgorithm::fragmentReceived (const Ptr<Packet> &f)
{
  SeqTsSizeFragHeader header;
  f->PeekHeader (header);

  uint64_t m_fragment_size = f->GetSize ();

  if (m_startedAt == Seconds (0))
    {
      m_startedAt = Simulator::Now ();
    }

  Time delay = Simulator::Now () - header.GetTs ();

  m_rateBuffer.insert (std::pair<Time, std::pair<uint32_t, Time>> (
      Simulator::Now (), std::pair<uint32_t, Time> (m_fragment_size, delay)));

  DataRate instant_throughput = DataRate (
      m_fragment_size * 8 / (Simulator::Now () - m_lastFragmentTime).GetSeconds ());

  uint64_t bytes = 0;
  Time sum_time = Seconds (0);
  Time window = MilliSeconds (140);

  double xsum = 0, x2sum = 0, ysum = 0, xysum = 0, n = 0;

  for (auto it = m_rateBuffer.begin (); it != m_rateBuffer.end ();)
    {
      if (it->first < Simulator::Now () - window)
        {
          it = m_rateBuffer.erase (it);
        }
      else
        {
          bytes += std::get<0> (it->second);
          sum_time += std::get<1> (it->second);

          xsum += it->first.GetSeconds (); //calculate sigma(xi)
          ysum += (std::get<1> (it->second)).GetSeconds (); //calculate sigma(yi)
          x2sum += std::pow (it->first.GetSeconds (), 2); //calculate sigma(x^2i)
          xysum += it->first.GetSeconds () *
                   (std::get<1> (it->second)).GetSeconds (); //calculate sigma(xi*yi)
          n += 1;

          ++it;
        }
    }

  double a = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum); //calculate slope
  double b = (x2sum * ysum - xsum * xysum) / (x2sum * n - xsum * xsum); //calculate intercept

  NS_LOG_DEBUG ("a: " << a);
  NS_LOG_DEBUG ("b: " << b);

  Time diff_delay = Seconds (window.GetSeconds () * a);

  NS_LOG_DEBUG ("Curr delay: " << delay);

  DataRate avg_throughput = DataRate (bytes * 8 /
                                      (Simulator::Now () - m_startedAt > window
                                           ? window
                                           : (Simulator::Now () - m_startedAt))
                                          .GetSeconds ());

  Time avg_delay = Seconds (sum_time.GetSeconds () / m_rateBuffer.size ());

  m_lastFragmentTime = Simulator::Now ();


  DataRate arate = fuzzyAlgorithm (avg_delay, diff_delay, avg_throughput);
  NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << " sec delay "
                        << delay.GetMilliSeconds () << " msec avg_delay "
                        << avg_delay.GetMilliSeconds () << " msec diff_delay "
                        << diff_delay.GetMilliSeconds () << " msec instant_throughput "
                        << instant_throughput.GetBitRate () / 1e6 << " Mbps avg_throughput "
                        << avg_throughput.GetBitRate () / 1e6 << " Mbps req_throughput "
                        << arate.GetBitRate () / 1e6 << " Mbps");



  return arate;
}

DataRate
FuzzyAlgorithm::fuzzyAlgorithm (Time delay, Time diffDelay, DataRate avgRate)
{
  NS_LOG_FUNCTION (this << delay << diffDelay << avgRate);

  double slow = 0, ok = 0, fast = 0, falling = 0, steady = 0, rising = 0, r1 = 0, r2 = 0, r3 = 0,
         r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0, p2 = 0, p1 = 0, z = 0, n1 = 0, n2 = 0,
         output = 0;

  Time target_delay = MilliSeconds (10);

  double t = target_delay.GetSeconds ();
  double t_diff = target_delay.GetSeconds ();

  double currDt = delay.GetSeconds ();
  double diff = diffDelay.GetSeconds ();

  if (currDt < 2 * t / 3)
    {
      slow = 1.0;
    }
  else if (currDt < t)
    {
      slow = 1 - 1 / (t / 3) * (currDt - 2 * t / 3);
      ok = 1 / (t / 3) * (currDt - 2 * t / 3);
    }
  else if (currDt < 4 * t)
    {
      ok = 1 - 1 / (3 * t) * (currDt - t);
      fast = 1 / (3 * t) * (currDt - t);
    }
  else
    {
      fast = 1;
    }

  if (diff < -2 * t_diff / 3)
    {
      falling = 1;
    }
  else if (diff < 0)
    {
      falling = 1 - 1 / (2 * t_diff / 3) * (diff + 2 * t_diff / 3);
      steady = 1 / (2 * t_diff / 3) * (diff + 2 * t_diff / 3);
    }
  else if (diff < 4 * t_diff)
    {
      steady = 1 - 1 / (4 * t_diff) * diff;
      rising = 1 / (4 * t_diff) * diff;
    }
  else
    {
      rising = 1;
    }

  r9 = std::min (slow, falling);
  r8 = std::min (ok, falling);
  r7 = std::min (fast, falling);
  r6 = std::min (slow, steady);
  r5 = std::min (ok, steady);
  r4 = std::min (fast, steady);
  r3 = std::min (slow, rising);
  r2 = std::min (ok, rising);
  r1 = std::min (fast, rising);

  p2 = std::sqrt (std::pow (r9, 2));
  p1 = std::sqrt (std::pow (r6, 2) + std::pow (r8, 2));
  z = std::sqrt (std::pow (r3, 2) + std::pow (r5, 2) + std::pow (r7, 2));
  n1 = std::sqrt (std::pow (r2, 2) + std::pow (r4, 2));
  n2 = std::sqrt (std::pow (r1, 2));

  /*output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 1.4 + p2 * 2)*/
  output = (n2 * 0.25 + n1 * 0.5 + z * 1 + p1 * 2 + p2 * 4) / (n2 + n1 + z + p1 + p2);

  NS_LOG_DEBUG ("slow " << slow << " ok " << ok << " fast " << fast << " falling " << falling
                        << " steady " << steady << " rising " << rising << " output " << output);

  return DataRate (output * avgRate.GetBitRate ());
}

} // namespace ns3
