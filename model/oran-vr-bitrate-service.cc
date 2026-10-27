#include "oran-vr-bitrate-service.h"

#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cstdint>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranVrBitrateService");

namespace
{

void
Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        p[i] = (v >> (24 - 8 * i)) & 0xff;
    }
}

uint32_t
Get32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void
Put16(uint8_t* p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v & 0xff;
}

uint16_t
Get16(const uint8_t* p)
{
    return (uint16_t(p[0]) << 8) | p[1];
}

} // namespace

NS_OBJECT_ENSURE_REGISTERED(OranVrBitrateServer);

TypeId
OranVrBitrateServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OranVrBitrateServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<OranVrBitrateServer>()
            .AddAttribute("OranLogicVrBitrate",
                          "The OranLogicVrBitrate module that computes the bitrates.",
                          PointerValue(),
                          MakePointerAccessor(&OranVrBitrateServer::m_lm),
                          MakePointerChecker<OranLogicVrBitrate>())
            .AddAttribute("Port",
                          "UDP port to listen on.",
                          UintegerValue(9000),
                          MakeUintegerAccessor(&OranVrBitrateServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("ProcessingDelay",
                          "Emulated xApp processing time before answering a request.",
                          TimeValue(Seconds(0)),
                          MakeTimeAccessor(&OranVrBitrateServer::m_processingDelay),
                          MakeTimeChecker());
    return tid;
}

OranVrBitrateServer::OranVrBitrateServer()
{
    NS_LOG_FUNCTION(this);
}

OranVrBitrateServer::~OranVrBitrateServer()
{
    NS_LOG_FUNCTION(this);
}

void
OranVrBitrateServer::DoDispose()
{
    m_socket = nullptr;
    m_lm = nullptr;
    Application::DoDispose();
}

void
OranVrBitrateServer::StartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_lm, "OranVrBitrateServer requires the OranLogicVrBitrate attribute");
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
        if (m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port)) == -1)
        {
            NS_FATAL_ERROR("OranVrBitrateServer failed to bind to port " << m_port);
        }
    }
    m_socket->SetRecvCallback(MakeCallback(&OranVrBitrateServer::HandleRead, this));
}

void
OranVrBitrateServer::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
OranVrBitrateServer::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        if (packet->GetSize() != REQUEST_SIZE)
        {
            NS_LOG_WARN("Ignoring malformed request of " << packet->GetSize() << " bytes");
            continue;
        }
        uint8_t buf[REQUEST_SIZE];
        packet->CopyData(buf, REQUEST_SIZE);

        const uint32_t reqId = Get32(buf);
        const Ipv4Address ueIp(Get32(buf + 4));
        const uint16_t uePort = Get16(buf + 8);
        const uint16_t lcid = Get16(buf + 10);
        const bool noQueue = buf[12] != 0;

        Simulator::Schedule(m_processingDelay,
                            &OranVrBitrateServer::Respond,
                            this,
                            socket,
                            from,
                            reqId,
                            Address(InetSocketAddress(ueIp, uePort)),
                            lcid,
                            noQueue);
    }
}

void
OranVrBitrateServer::Respond(Ptr<Socket> socket,
                             Address from,
                             uint32_t reqId,
                             Address ue,
                             uint16_t lcid,
                             bool noQueue)
{
    const DataRate rate = noQueue ? m_lm->GetVrBitrateNoTxQueue(ue, lcid)
                                  : m_lm->GetVrBitrate(ue, lcid);
    NS_LOG_DEBUG("Answering request " << reqId << " with " << rate.GetBitRate() / 1e6 << " Mbps");

    uint8_t buf[RESPONSE_SIZE];
    Put32(buf, reqId);
    const uint64_t bps = rate.GetBitRate();
    Put32(buf + 4, bps >> 32);
    Put32(buf + 8, bps & 0xffffffff);
    socket->SendTo(Create<Packet>(buf, RESPONSE_SIZE), 0, from);
}

NS_OBJECT_ENSURE_REGISTERED(OranVrBitrateClient);

TypeId
OranVrBitrateClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::OranVrBitrateClient")
            .SetParent<Object>()
            .SetGroupName("Applications")
            .AddConstructor<OranVrBitrateClient>()
            .AddAttribute("MaxAge",
                          "Answers older than this are considered stale and not used.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&OranVrBitrateClient::m_maxAge),
                          MakeTimeChecker())
            .AddAttribute("RequestTimeout",
                          "Time after which an unanswered request is considered lost.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&OranVrBitrateClient::m_requestTimeout),
                          MakeTimeChecker());
    return tid;
}

OranVrBitrateClient::OranVrBitrateClient()
{
    NS_LOG_FUNCTION(this);
}

OranVrBitrateClient::~OranVrBitrateClient()
{
    NS_LOG_FUNCTION(this);
}

void
OranVrBitrateClient::DoDispose()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }
    Object::DoDispose();
}

void
OranVrBitrateClient::Setup(Ptr<Node> socketNode, const Address& server)
{
    NS_LOG_FUNCTION(this << socketNode << server);
    m_server = server;
    m_socket = Socket::CreateSocket(socketNode, TypeId::LookupByName("ns3::UdpSocketFactory"));
    m_socket->Bind();
    m_socket->SetRecvCallback(MakeCallback(&OranVrBitrateClient::HandleRead, this));
}

void
OranVrBitrateClient::Request(const Address& ue, uint16_t lcid, bool noQueue)
{
    NS_LOG_FUNCTION(this << lcid << noQueue);
    NS_ABORT_MSG_IF(!m_socket, "OranVrBitrateClient::Setup() must be called first");
    NS_ABORT_MSG_IF(!InetSocketAddress::IsMatchingType(ue), "Only IPv4 UE addresses supported");

    if (m_pendingReqId != 0 && Simulator::Now() - m_pendingSince < m_requestTimeout)
    {
        return;
    }

    const InetSocketAddress ueAddr = InetSocketAddress::ConvertFrom(ue);
    m_pendingReqId = m_nextReqId++;
    m_pendingSince = Simulator::Now();

    uint8_t buf[OranVrBitrateServer::REQUEST_SIZE] = {0};
    Put32(buf, m_pendingReqId);
    Put32(buf + 4, ueAddr.GetIpv4().Get());
    Put16(buf + 8, ueAddr.GetPort());
    Put16(buf + 10, lcid);
    buf[12] = noQueue ? 1 : 0;
    m_socket->SendTo(Create<Packet>(buf, OranVrBitrateServer::REQUEST_SIZE), 0, m_server);
}

void
OranVrBitrateClient::HandleRead(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        if (packet->GetSize() != OranVrBitrateServer::RESPONSE_SIZE)
        {
            NS_LOG_WARN("Ignoring malformed response of " << packet->GetSize() << " bytes");
            continue;
        }
        uint8_t buf[OranVrBitrateServer::RESPONSE_SIZE];
        packet->CopyData(buf, OranVrBitrateServer::RESPONSE_SIZE);

        const uint32_t reqId = Get32(buf);
        if (reqId == m_pendingReqId)
        {
            m_pendingReqId = 0;
        }
        const uint64_t bps = (uint64_t(Get32(buf + 4)) << 32) | Get32(buf + 8);
        m_latest = DataRate(bps);
        m_latestTime = Simulator::Now();
        m_hasLatest = true;
        NS_LOG_DEBUG("Received bitrate " << bps / 1e6 << " Mbps for request " << reqId);
    }
}

DataRate
OranVrBitrateClient::GetLatestBitrate() const
{
    if (!m_hasLatest || Simulator::Now() - m_latestTime > m_maxAge)
    {
        return DataRate(0);
    }
    return m_latest;
}

} // namespace ns3
