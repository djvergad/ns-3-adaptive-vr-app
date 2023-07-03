/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
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
 */
#include "tcp-stream-adaptation-algorithm.h"

#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AdaptationAlgorithm");

NS_OBJECT_ENSURE_REGISTERED(AdaptationAlgorithm);

AdaptationAlgorithm::AdaptationAlgorithm()
{
    m_videoData.segmentDuration = 2000000;
    m_videoData.averageBitrate = { 3128000, 3254000, 3974000, 4496000, 6408000, 10938000, 17156000, 35018000 };
}

DataRate
AdaptationAlgorithm::adaptation_algorithm (double buffOcc, double diffBuffOcc, DataRate lastRate)
{
  m_bufferData.bufferLevelNew.push_back(1000000 * 8 * (buffOcc) / lastRate.GetBitRate()  );
  m_bufferData.timeNow.push_back(Simulator::Now().GetMicroSeconds());
  
  algorithmReply reply = GetNextRep(m_segmentCounter++, 0);
  std::cout << reply.nextRepIndex << std::endl;
  m_playbackData.playbackIndex.push_back(reply.nextRepIndex);
  return m_videoData.averageBitrate[reply.nextRepIndex];
}

DataRate
AdaptationAlgorithm::nextBurstRate (Ptr<Socket> socket, uint64_t bytesAddedToSocket,
                                          Time txTime)
{
  m_throughput.bytesReceived.push_back(bytesAddedToSocket);
  m_throughput.transmissionRequested.push_back((Simulator::Now() - txTime).GetMicroSeconds());
  m_throughput.transmissionStart.push_back((Simulator::Now() - txTime).GetMicroSeconds());
  m_throughput.transmissionEnd.push_back(Simulator::Now().GetMicroSeconds());
  return AdaptationAlgorithmServer::nextBurstRate(socket, bytesAddedToSocket, txTime);
}
} // namespace ns3