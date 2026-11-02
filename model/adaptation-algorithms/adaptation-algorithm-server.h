#ifndef ADAPTATION_ALGORITHM_SERVER_H
#define ADAPTATION_ALGORITHM_SERVER_H

#include "ns3/data-rate.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/tcp-socket-base.h"

namespace ns3
{

class AdaptationAlgorithmServer : public Object
{
  public:
    static TypeId GetTypeId(void);

    AdaptationAlgorithmServer();
    virtual ~AdaptationAlgorithmServer();

    virtual DataRate nextBurstRate(Ptr<Socket> socket, uint64_t bytesAddedToSocket, Time txTime);
    /**
     * @brief Set the controller's initial bitrate.
     * @param rate The initial media bitrate.
     */
    virtual void SetInitialRate(DataRate rate);

    /**
     * @brief Provide receiver-side network measurements to the controller.
     * @param queueDelay Estimated one-way queueing delay.
     * @param delayGradient Difference between arrival and departure intervals.
     * @param receiveRate Estimated receiver bitrate.
     * @param lossFraction Estimated packet loss fraction in [0, 1].
     */
    virtual void ReportFeedback(Time queueDelay,
                                Time delayGradient,
                                DataRate receiveRate,
                                double lossFraction);

    /**
     * @brief Get the current target bitrate.
     * @return The controller's target bitrate.
     */
    DataRate GetTargetRate() const;

  protected:
    virtual DataRate adaptation_algorithm(double buff_occ,
                                          double diff_buff_occ,
                                          DataRate lastRate) = 0;

    DataRate m_targetRate = DataRate("100kbps"); ///< Current controller target bitrate.
    Time m_queueDelay = Seconds(0);              ///< Latest reported queueing delay.
    Time m_delayGradient = Seconds(0);           ///< Latest reported delay gradient.
    DataRate m_receiveRate = DataRate(0);        ///< Latest reported receive bitrate.
    double m_lossFraction = 0.0;                 ///< Latest reported loss fraction.

    Time m_lastBurstTime = Seconds(0);

    uint64_t m_lastBufferOcc = 0;
};

} // namespace ns3

#endif /* ADAPTATION_ALGORITHM_SERVER_H */
