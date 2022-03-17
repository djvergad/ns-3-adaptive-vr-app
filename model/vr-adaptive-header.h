/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 TEI of Western Macedonia, Greece
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
 * Author: Dimitrios J. Vergados <djvergad@gmail.com>
 */

#ifndef VR_ADAPTIVE_HEADER_H
#define VR_ADAPTIVE_HEADER_H

#include <ns3/data-rate.h>
#include <ns3/seq-ts-header.h>

namespace ns3 {

#define VR_ADAPTIVE_REQUEST 0
#define VR_ADAPTIVE_RESPONSE 1

class VrAdaptiveHeader : public SeqTsHeader
{
public:
  static TypeId GetTypeId (void);

  VrAdaptiveHeader ();

  void SetTargetDataRate (DataRate targetDataRate);
  DataRate GetTargetDataRate (void) const;

  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:
  DataRate m_targetDataRate;
};

} // namespace ns3

#endif /* VR_ADAPTIVE_HEADER_H */
