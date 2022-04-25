#include "adaptation-algorithm-server.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AdaptationAlgorithmServer");

NS_OBJECT_ENSURE_REGISTERED (AdaptationAlgorithmServer);

TypeId
AdaptationAlgorithmServer::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::AdaptationAlgorithmServer").SetParent<Object> ().SetGroupName ("Applications")
      // .AddConstructor<AdaptationAlgorithmServer> ()
      ;
  return tid;
}

AdaptationAlgorithmServer::AdaptationAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

AdaptationAlgorithmServer::~AdaptationAlgorithmServer ()
{
  NS_LOG_FUNCTION (this);
}

DataRate
AdaptationAlgorithmServer::nextBurstRate (Ptr<TcpSocketBase> socket, uint64_t bytesAddedToSocket,
                                          Time txTime)
{
  NS_LOG_FUNCTION (this << socket << bytesAddedToSocket);

  UintegerValue buf_size;
  socket->GetAttribute ("SndBufSize", buf_size);

  Time dt = Simulator::Now () - m_lastBurstTime;
  m_lastBurstTime = Simulator::Now ();

  uint64_t buff_occ = buf_size.Get () - socket->GetTxAvailable ();
  int128_t diff_buff_occ = buff_occ - m_lastBufferOcc;
  m_lastBufferOcc = buff_occ;

  uint64_t bytes_sent = bytesAddedToSocket - diff_buff_occ;
  // DataRate lastRate = DataRate (bytes_sent * 8 / dt.GetSeconds ());
  DataRate lastRate = DataRate (bytes_sent * 8 / txTime.GetSeconds ());

  std::cout << "bytes_sent " << bytes_sent << " txTime " << txTime.GetSeconds () << " dt "
            << dt.GetSeconds () << std::endl;

  NS_LOG_DEBUG ("buff_occ " << buff_occ << " diff_buff_occ " << (int) diff_buff_occ << " lastRate "
                            << lastRate.GetBitRate () / 1e6);

  if (txTime > Seconds (0))
    {
      return adaptation_algorithm (buff_occ, diff_buff_occ, lastRate);
    }
  else
    {
      return DataRate ("10Mbps");
    }
}

} // namespace ns3
