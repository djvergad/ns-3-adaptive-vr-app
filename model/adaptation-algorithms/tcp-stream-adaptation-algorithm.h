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
#ifndef ADAPTATION_ALGORITHM_H
#define ADAPTATION_ALGORITHM_H

#include "adaptation-algorithm-server.h"
#include "tcp-stream-interface.h"

#include "ns3/application.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <numeric>
#include <stdexcept>
#include <stdint.h>

namespace ns3
{
/**
 * \ingroup tcpStream
 * \brief A base class for adaptation algorithms
 *
 */
class AdaptationAlgorithm : public AdaptationAlgorithmServer
{
  public:
    AdaptationAlgorithm();

    /**
     * \ingroup tcpStream
     * \brief Compute the next representation index
     *
     * Every Adaptation algorithm must overwrite the method.
     *
     * \return struct containig the index of next representation to be downloaded and the
     * inter-request delay.
     */
    virtual algorithmReply GetNextRep(const int64_t segmentCounter, int64_t clientId) = 0;

    virtual DataRate nextBurstRate(Ptr<Socket> socket, uint64_t bytesAddedToSocket, Time txTime);

  protected:
    virtual DataRate adaptation_algorithm(double buff_occ, double diff_buff_occ, DataRate lastRate);

    videoData m_videoData;
    bufferData m_bufferData;
    throughputData m_throughput;
    playbackData m_playbackData;
    int64_t m_segmentCounter = 0;
};
} // namespace ns3

#endif /* ADAPTATION_ALGORITHM_H */