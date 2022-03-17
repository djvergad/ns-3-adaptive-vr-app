/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 INRIA
 * Copyright (c) 2018 Natale Patriciello <natale.patriciello@gmail.com>
 *                    (added timestamp and size fields)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ns3/log.h"
#include "vr-adaptive-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VrAdaptiveHeader");

NS_OBJECT_ENSURE_REGISTERED (VrAdaptiveHeader);

VrAdaptiveHeader::VrAdaptiveHeader () : SeqTsHeader ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
VrAdaptiveHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::VrAdaptiveHeader")
                          .SetParent<SeqTsHeader> ()
                          .SetGroupName ("Applications")
                          .AddConstructor<VrAdaptiveHeader> ();
  return tid;
}

TypeId
VrAdaptiveHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
VrAdaptiveHeader::SetTargetDataRate (DataRate targetDataRate)
{
  m_targetDataRate = targetDataRate;
}

DataRate
VrAdaptiveHeader::GetTargetDataRate (void) const
{
  return m_targetDataRate;
}

void
VrAdaptiveHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "(size=" << m_targetDataRate << ") AND ";
  SeqTsHeader::Print (os);
}

uint32_t
VrAdaptiveHeader::GetSerializedSize (void) const
{
  return SeqTsHeader::GetSerializedSize () + sizeof (DataRate);
}

void
VrAdaptiveHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteHtonU64 (m_targetDataRate.GetBitRate ());
  SeqTsHeader::Serialize (i);
}

uint32_t
VrAdaptiveHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_targetDataRate = DataRate (i.ReadNtohU64 ());
  SeqTsHeader::Deserialize (i);
  return GetSerializedSize ();
}

} // namespace ns3
