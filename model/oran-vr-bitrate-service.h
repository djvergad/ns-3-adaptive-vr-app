#ifndef ORAN_VR_BITRATE_SERVICE_H
#define ORAN_VR_BITRATE_SERVICE_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/oran-module.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <map>

namespace ns3
{

/**
 * Request/response service that exposes OranLogicVrBitrate over the network.
 *
 * The server runs on the node hosting the RIC xApp. It receives small UDP
 * request messages (UE address, LCID, query type) and answers with the bitrate
 * computed by the OranLogicVrBitrate logic module. Requests and responses
 * traverse the simulated network and therefore experience its delay and loss.
 */
class OranVrBitrateServer : public Application
{
  public:
    /**
     * Get the type ID.
     *
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OranVrBitrateServer();
    ~OranVrBitrateServer() override;

    /** Size in bytes of a request message. */
    static constexpr uint32_t REQUEST_SIZE = 17;
    /** Size in bytes of a response message. */
    static constexpr uint32_t RESPONSE_SIZE = 12;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * Handle a received request.
     *
     * @param socket The socket with a pending packet.
     */
    void HandleRead(Ptr<Socket> socket);

    /**
     * Compute the bitrate and send the response.
     *
     * @param socket The socket to reply on.
     * @param from The requester address.
     * @param reqId The request identifier to echo.
     * @param ue The UE address the bitrate is requested for.
     * @param lcid The logical channel ID.
     * @param noQueue True to ignore the tx queue state.
     */
    void Respond(Ptr<Socket> socket,
                 Address from,
                 uint32_t reqId,
                 Address ue,
                 uint16_t lcid,
                 bool noQueue);

    Ptr<OranLogicVrBitrate> m_lm; ///< Logic module computing the bitrates
    uint16_t m_port;              ///< UDP port to listen on
    Time m_processingDelay;       ///< Emulated xApp processing time
    Ptr<Socket> m_socket;         ///< Listening socket
};

/**
 * Client side of the OranVrBitrateServer service.
 *
 * Requests are asynchronous: Request() sends a message to the server and the
 * latest received answer is available through GetLatestBitrate().
 */
class OranVrBitrateClient : public Object
{
  public:
    /**
     * Get the type ID.
     *
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    OranVrBitrateClient();
    ~OranVrBitrateClient() override;

    /**
     * Configure the client.
     *
     * @param socketNode The node to open the UDP socket on.
     * @param server The server address (Ipv4 address and port).
     */
    void Setup(Ptr<Node> socketNode, const Address& server);

    /**
     * Send a bitrate request unless one is already outstanding.
     *
     * @param ue The UE address the bitrate is requested for.
     * @param lcid The logical channel ID.
     * @param noQueue True to ignore the tx queue state.
     */
    void Request(const Address& ue, uint16_t lcid, bool noQueue);

    /**
     * Get the most recent bitrate received from the server.
     *
     * @return The bitrate, or DataRate(0) if no valid answer has arrived (or the
     * answer is older than the MaxAge attribute).
     */
    DataRate GetLatestBitrate() const;

  protected:
    void DoDispose() override;

  private:
    /**
     * Handle a received response.
     *
     * @param socket The socket with a pending packet.
     */
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;          ///< UDP socket
    Address m_server;              ///< Server address
    Time m_maxAge;                 ///< Maximum age of a usable answer
    Time m_requestTimeout;         ///< Time after which a lost request is retried
    uint32_t m_nextReqId{1};       ///< Next request identifier
    uint32_t m_pendingReqId{0};    ///< Outstanding request identifier, 0 if none
    Time m_pendingSince;           ///< Time the outstanding request was sent
    DataRate m_latest{0};          ///< Last received bitrate
    Time m_latestTime{Seconds(0)}; ///< Reception time of the last bitrate
    bool m_hasLatest{false};       ///< True once an answer has been received
};

} // namespace ns3

#endif // ORAN_VR_BITRATE_SERVICE_H
