/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "congestion-feedback-header.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CongestionFeedbackHeader");
NS_OBJECT_ENSURE_REGISTERED(CongestionFeedbackHeader);

TypeId
CongestionFeedbackHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::CongestionFeedbackHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<CongestionFeedbackHeader>();
    return tid;
}

TypeId
CongestionFeedbackHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
CongestionFeedbackHeader::GetSerializedSize() const
{
    return 32;
}

void
CongestionFeedbackHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(m_magic);
    start.WriteHtonU64(m_queueDelayNs);
    start.WriteHtonU64(static_cast<uint64_t>(m_delayGradientNs));
    start.WriteHtonU64(m_receiveRate);
    start.WriteHtonU32(m_lossFractionPpm);
}

uint32_t
CongestionFeedbackHeader::Deserialize(Buffer::Iterator start)
{
    m_magic = start.ReadNtohU32();
    m_queueDelayNs = start.ReadNtohU64();
    m_delayGradientNs = static_cast<int64_t>(start.ReadNtohU64());
    m_receiveRate = start.ReadNtohU64();
    m_lossFractionPpm = start.ReadNtohU32();
    return GetSerializedSize();
}

void
CongestionFeedbackHeader::Print(std::ostream& os) const
{
    os << "queueDelay=" << GetQueueDelay().As(Time::MS)
       << "ms delayGradient=" << GetDelayGradient().As(Time::MS)
       << "ms receiveRate=" << GetReceiveRate().GetBitRate()
       << "bps lossFraction=" << GetLossFraction();
}

bool
CongestionFeedbackHeader::IsValid() const
{
    return m_magic == MAGIC && m_lossFractionPpm <= 1000000;
}

void
CongestionFeedbackHeader::SetMetrics(Time queueDelay,
                                     Time delayGradient,
                                     DataRate receiveRate,
                                     double lossFraction)
{
    m_queueDelayNs = static_cast<uint64_t>(std::max<int64_t>(0, queueDelay.GetNanoSeconds()));
    m_delayGradientNs = delayGradient.GetNanoSeconds();
    m_receiveRate = receiveRate.GetBitRate();
    m_lossFractionPpm =
        static_cast<uint32_t>(std::lround(std::clamp(lossFraction, 0.0, 1.0) * 1000000.0));
}

Time
CongestionFeedbackHeader::GetQueueDelay() const
{
    return NanoSeconds(m_queueDelayNs);
}

Time
CongestionFeedbackHeader::GetDelayGradient() const
{
    return NanoSeconds(m_delayGradientNs);
}

DataRate
CongestionFeedbackHeader::GetReceiveRate() const
{
    return DataRate(m_receiveRate);
}

double
CongestionFeedbackHeader::GetLossFraction() const
{
    return static_cast<double>(m_lossFractionPpm) / 1000000.0;
}

} // namespace ns3
