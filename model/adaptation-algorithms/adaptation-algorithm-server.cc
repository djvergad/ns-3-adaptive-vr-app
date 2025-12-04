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
  // Prefer to query TCP sockets for SndBufSize; UDP sockets do not expose this
  // attribute. If the socket is not TCP, fall back to using the current
  // available tx space as the buffer size so occupancy calculations still work.
  Ptr<TcpSocketBase> tcp = DynamicCast<TcpSocketBase>(socket);
  if (tcp)
    {
      tcp->GetAttribute("SndBufSize", buf_size);
    }
  else
    {
      // Fallback: treat current available tx space as the buffer size (so
      // buffer occupancy becomes zero). This is conservative for UDP.
      buf_size = UintegerValue(socket->GetTxAvailable());
    }
  


  Time dt = Simulator::Now () - m_lastBurstTime;
  m_lastBurstTime = Simulator::Now ();

  uint64_t buffOcc = buf_size.Get () - socket->GetTxAvailable ();
  int128_t diffBuffOcc = buffOcc - m_lastBufferOcc;
  m_lastBufferOcc = buffOcc;

  uint64_t bytesSent = bytesAddedToSocket - diffBuffOcc;
  // DataRate lastRate = DataRate (bytesSent * 8 / dt.GetSeconds ());
  DataRate lastRate = DataRate (bytesSent * 8 / txTime.GetSeconds ());

  // std::cout << "bytesSent " << bytesSent << " txTime " << txTime.GetSeconds () << " dt "
  //           << dt.GetSeconds () << std::endl;

  NS_LOG_DEBUG ("buffOcc " << buffOcc << " diffBuffOcc " << (int) diffBuffOcc << " lastRate "
                            << lastRate.GetBitRate () / 1e6);
  if (txTime > Seconds (0) || !tcp)
    {
      return adaptation_algorithm (buffOcc, diffBuffOcc, lastRate);
    }
  else
    {
      return DataRate ("10Mbps");
    }
}

} // namespace ns3
