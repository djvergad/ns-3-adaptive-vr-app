/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ns3/congestion-feedback-header.h"
#include "ns3/gcc-algorithm-server.h"
#include "ns3/nada-algorithm-server.h"
#include "ns3/test.h"

namespace ns3
{

class CongestionFeedbackHeaderTestCase : public TestCase
{
  public:
    CongestionFeedbackHeaderTestCase()
        : TestCase("Congestion feedback header preserves measurements")
    {
    }

  private:
    void DoRun() override
    {
        CongestionFeedbackHeader expected;
        expected.SetMetrics(MilliSeconds(23), NanoSeconds(-4000000), DataRate("8Mbps"), 0.125);

        Ptr<Packet> packet = Create<Packet>();
        packet->AddHeader(expected);
        CongestionFeedbackHeader actual;
        NS_TEST_ASSERT_MSG_EQ(packet->RemoveHeader(actual),
                              expected.GetSerializedSize(),
                              "Unexpected congestion feedback header size");
        NS_TEST_ASSERT_MSG_EQ(actual.IsValid(), true, "Feedback header should be valid");
        NS_TEST_ASSERT_MSG_EQ(actual.GetQueueDelay(), MilliSeconds(23), "Queue delay changed");
        NS_TEST_ASSERT_MSG_EQ(actual.GetDelayGradient(),
                              NanoSeconds(-4000000),
                              "Delay gradient changed");
        NS_TEST_ASSERT_MSG_EQ(actual.GetReceiveRate().GetBitRate(),
                              DataRate("8Mbps").GetBitRate(),
                              "Receive rate changed");
        NS_TEST_ASSERT_MSG_EQ_TOL(actual.GetLossFraction(), 0.125, 0.000001, "Loss changed");
    }
};

class GccFeedbackTestCase : public TestCase
{
  public:
    GccFeedbackTestCase()
        : TestCase("GCC reduces on delay overuse and increases on underuse")
    {
    }

  private:
    void DoRun() override
    {
        GccAlgorithmServer algorithm;
        algorithm.SetInitialRate(DataRate("10Mbps"));
        algorithm.ReportFeedback(Seconds(0), MilliSeconds(50), DataRate("9Mbps"), 0.0);
        const DataRate overuseRate = algorithm.GetTargetRate();
        NS_TEST_ASSERT_MSG_LT(overuseRate.GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "GCC should lower its target during delay overuse");
        NS_TEST_ASSERT_MSG_EQ(overuseRate.GetBitRate() >= DataRate("10kbps").GetBitRate(),
                              true,
                              "GCC target should respect its lower bound");

        for (uint32_t i = 0; i < 30; ++i)
        {
            algorithm.ReportFeedback(Seconds(0), NanoSeconds(-50000000), DataRate("9Mbps"), 0.0);
        }
        NS_TEST_ASSERT_MSG_GT(algorithm.GetTargetRate().GetBitRate(),
                              overuseRate.GetBitRate(),
                              "GCC should recover when feedback indicates underuse");

        GccAlgorithmServer lossController;
        lossController.SetInitialRate(DataRate("10Mbps"));
        lossController.ReportFeedback(Seconds(0), Seconds(0), DataRate("9Mbps"), 0.2);
        NS_TEST_ASSERT_MSG_LT(lossController.GetTargetRate().GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "GCC should reduce rate when packet loss exceeds 10 percent");
    }
};

class NadaFeedbackTestCase : public TestCase
{
  public:
    NadaFeedbackTestCase()
        : TestCase("NADA combines queue delay and loss and ramps up when clear")
    {
    }

  private:
    void DoRun() override
    {
        NadaAlgorithmServer algorithm;
        algorithm.SetInitialRate(DataRate("10Mbps"));
        algorithm.ReportFeedback(MilliSeconds(200), Seconds(0), DataRate("9Mbps"), 0.03);
        const DataRate congestedRate = algorithm.GetTargetRate();
        NS_TEST_ASSERT_MSG_LT(congestedRate.GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "NADA should reduce its target for a high congestion signal");

        algorithm.ReportFeedback(MilliSeconds(200), Seconds(0), DataRate("9Mbps"), 0.0);
        NS_TEST_ASSERT_MSG_GT(algorithm.GetTargetRate().GetBitRate(),
                              congestedRate.GetBitRate(),
                              "NADA should ramp up when its warped delay signal clears");

        algorithm.ReportFeedback(Seconds(0), Seconds(0), DataRate("9Mbps"), 0.10);
        NS_TEST_ASSERT_MSG_LT(algorithm.GetTargetRate().GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "NADA should include packet loss in its congestion signal");
    }
};

class GccNadaAdaptationAlgorithmTestSuite : public TestSuite
{
  public:
    GccNadaAdaptationAlgorithmTestSuite()
        : TestSuite("gcc-nada-adaptation-algorithms", Type::UNIT)
    {
        AddTestCase(new CongestionFeedbackHeaderTestCase(), TestCase::Duration::QUICK);
        AddTestCase(new GccFeedbackTestCase(), TestCase::Duration::QUICK);
        AddTestCase(new NadaFeedbackTestCase(), TestCase::Duration::QUICK);
    }
};

static GccNadaAdaptationAlgorithmTestSuite g_gccNadaAdaptationAlgorithmTestSuite;

} // namespace ns3
