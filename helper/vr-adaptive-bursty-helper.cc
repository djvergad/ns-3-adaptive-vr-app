/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 SIGNET Lab, Department of Information Engineering,
 * University of Padova
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
#include "vr-adaptive-bursty-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/data-rate.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/random-variable-stream.h"
#include "ns3/vr-adaptive-bursty-application.h"
#include "ns3/simple-burst-generator.h"
#include "ns3/vr-burst-generator.h"

namespace ns3 {

VrAdaptiveBurstyHelper::VrAdaptiveBurstyHelper (std::string protocol, Address address)
{
  m_burstyApplicationFactory.SetTypeId ("ns3::VrAdaptiveBurstyApplication");
  m_burstyApplicationFactory.Set ("Protocol", StringValue (protocol));
  m_burstyApplicationFactory.Set ("Remote", AddressValue (address));
}

void
VrAdaptiveBurstyHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_burstyApplicationFactory.Set (name, value);
}

void 
VrAdaptiveBurstyHelper::SetBurstGenerator (std::string type,
                                 std::string n1, const AttributeValue &v1,
                                 std::string n2, const AttributeValue &v2,
                                 std::string n3, const AttributeValue &v3,
                                 std::string n4, const AttributeValue &v4)
{
  m_burstGeneratorFactory.SetTypeId (type);
  m_burstGeneratorFactory.Set (n1, v1);
  m_burstGeneratorFactory.Set (n2, v2);
  m_burstGeneratorFactory.Set (n3, v3);
  m_burstGeneratorFactory.Set (n4, v4);
}

void
VrAdaptiveBurstyHelper::SetBurstGeneratorAttribute (std::string name, const AttributeValue &value)
{
  m_burstGeneratorFactory.Set (name, value);
}

ApplicationContainer
VrAdaptiveBurstyHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
VrAdaptiveBurstyHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
VrAdaptiveBurstyHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
VrAdaptiveBurstyHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<VrAdaptiveBurstyApplication> app = m_burstyApplicationFactory.Create<VrAdaptiveBurstyApplication> ();
  Ptr<BurstGenerator> generator = m_burstGeneratorFactory.Create<BurstGenerator> ();

  app->SetAttribute ("BurstGenerator", PointerValue (generator));
  node->AddApplication (app);

  return app;
}

int64_t
VrAdaptiveBurstyHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      for (uint32_t j = 0; j < node->GetNApplications (); j++)
        {
          Ptr<VrAdaptiveBurstyApplication> app = DynamicCast<VrAdaptiveBurstyApplication> (node->GetApplication (j));
          if (app)
            {
              Ptr<SimpleBurstGenerator> simpleGenerator =
                  DynamicCast<SimpleBurstGenerator> (app->GetBurstGenerator ()); // TODO improve
              if (simpleGenerator)
                {
                  currentStream += simpleGenerator->AssignStreams (currentStream);
                }
              Ptr<VrBurstGenerator> vrGenerator =
                  DynamicCast<VrBurstGenerator> (app->GetBurstGenerator ()); // TODO improve
              if (vrGenerator)
                {
                  currentStream += vrGenerator->AssignStreams (currentStream);
                }
            }
        }
    }
  return (currentStream - stream);
}

} // namespace ns3
