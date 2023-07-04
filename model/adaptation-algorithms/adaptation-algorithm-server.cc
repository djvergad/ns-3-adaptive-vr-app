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
AdaptationAlgorithmServer::nextBurstRate (Ptr<Socket> socket, uint64_t bytesAddedToSocket,
                                          Time txTime)
{
  NS_LOG_FUNCTION (this << socket << bytesAddedToSocket);

  UintegerValue buf_size;
  DynamicCast<TcpSocketBase> (socket)->GetAttribute ("SndBufSize", buf_size);
  


  Time dt = Simulator::Now () - m_lastBurstTime;
  m_lastBurstTime = Simulator::Now ();

  uint64_t buffOcc = buf_size.Get () - socket->GetTxAvailable ();
  int128_t diffBuffOcc = buffOcc - m_lastBufferOcc;
  m_lastBufferOcc = buffOcc;

  uint64_t bytesSent = bytesAddedToSocket - diffBuffOcc;
  // DataRate lastRate = DataRate (bytesSent * 8 / dt.GetSeconds ());
  DataRate lastRate = DataRate (bytesSent * 8 / txTime.GetSeconds ());

  std::cout << "bytesSent " << bytesSent << " txTime " << txTime.GetSeconds () << " dt "
            << dt.GetSeconds () << std::endl;

  NS_LOG_DEBUG ("buffOcc " << buffOcc << " diffBuffOcc " << (int) diffBuffOcc << " lastRate "
                            << lastRate.GetBitRate () / 1e6);

  if (txTime > Seconds (0))
    {
      return adaptation_algorithm (buffOcc, diffBuffOcc, lastRate);
    }
  else
    {
      return DataRate ("10Mbps");
    }
}

} // namespace ns3
