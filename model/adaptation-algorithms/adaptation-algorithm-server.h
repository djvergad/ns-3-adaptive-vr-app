#ifndef ADAPTATION_ALGORITHM_SERVER_H
#define ADAPTATION_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/oran-module.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/bursty-application-server-instance.h"

namespace ns3
{

class AdaptationAlgorithmServer : public Object
{
  public:
    static TypeId GetTypeId(void);

    AdaptationAlgorithmServer();
    virtual ~AdaptationAlgorithmServer();

    virtual DataRate nextBurstRate(Ptr<Socket> socket, uint64_t bytesAddedToSocket, Time txTime);

    DataRate m_maxDataRate = DataRate("0");

    Ptr<BurstyApplicationServerInstance> m_server_instance = nullptr;

  protected:
    virtual DataRate adaptation_algorithm(double buff_occ, double diff_buff_occ, DataRate lastRate);

    Time m_lastBurstTime = Seconds(0);

    uint64_t m_lastBufferOcc = 0;

    Ptr<OranLogicVrBitrate> m_lm =
        nullptr; // Pointer to the OranLogicVrBitrate instance for querying VR bitrates

    DataRate m_lastRequestedDataRate = DataRate("0");
};

} // namespace ns3

#endif /* ADAPTATION_ALGORITHM_SERVER_H */
