/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "ns3/congestion-feedback-header.h"
#include "ns3/gcc-algorithm-server.h"
#include "ns3/nada-algorithm-server.h"
#include "ns3/oran-logic-vr-bitrate.h"
#include "ns3/simulator.h"
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
        : TestCase("NADA follows the RFC 8698 rate update, warping and ramp-up rules")
    {
    }

  private:
    /**
     * @brief Deliver one feedback report at an absolute simulation time.
     * @param algorithm The controller.
     * @param when The time of the report.
     * @param delay The queueing delay.
     * @param rate The receive rate.
     * @param loss The loss fraction.
     */
    static void Report(Ptr<NadaAlgorithmServer> algorithm,
                       Time when,
                       Time delay,
                       DataRate rate,
                       double loss)
    {
        Simulator::Schedule(when - Simulator::Now(), [=]() {
            algorithm->ReportFeedback(delay, Seconds(0), rate, loss);
        });
        Simulator::Run();
    }

    /**
     * @brief Create a controller with a 50 Mb/s ceiling and a 10 Mb/s start.
     * @return The controller.
     */
    static Ptr<NadaAlgorithmServer> Create50Mbps()
    {
        Ptr<NadaAlgorithmServer> algorithm = CreateObject<NadaAlgorithmServer>();
        algorithm->SetInitialRate(DataRate("50Mbps"));
        return algorithm;
    }

    void DoRun() override
    {
        const DataRate recv("9Mbps");
        const Time step = MilliSeconds(100);

        // Without loss the delay is not warped, so a deep queue must keep lowering the rate
        // towards its equilibrium, including below the rate at which x_ref reaches QTH.
        Ptr<NadaAlgorithmServer> noLoss = Create50Mbps();
        Time now = Seconds(1);
        for (uint32_t i = 0; i < 60; ++i, now += step)
        {
            Report(noLoss, now, MilliSeconds(500), recv, 0.0);
        }
        NS_TEST_ASSERT_MSG_LT(noLoss->GetTargetRate().GetBitRate(),
                              DataRate("2Mbps").GetBitRate(),
                              "A standing queue without loss should push the rate down");

        // After a loss the same delay is warped and no longer lowers the rate.
        Ptr<NadaAlgorithmServer> warped = Create50Mbps();
        now += step;
        const Time start = now;
        Report(warped, start, Seconds(0), recv, 0.0);
        now = start + Seconds(1);
        Report(warped, now, Seconds(0), recv, 0.001); // loss_int = 1 s, loss_exp = 7 s
        for (uint32_t i = 0; i < 10; ++i)
        {
            now += step;
            Report(warped, now, MilliSeconds(500), recv, 0.0);
        }
        NS_TEST_ASSERT_MSG_GT(warped->GetTargetRate().GetBitRate(),
                              DataRate("5Mbps").GetBitRate(),
                              "The delay should be warped while a loss is recent");

        // Once the loss window has expired, the same delay settles at the unwarped equilibrium
        // of XREF * RMAX / x = 1 Mb/s.
        now = start + Seconds(9);
        for (uint32_t i = 0; i < 60; ++i, now += step)
        {
            Report(warped, now, MilliSeconds(500), recv, 0.0);
        }
        NS_TEST_ASSERT_MSG_LT(warped->GetTargetRate().GetBitRate(),
                              DataRate("2Mbps").GetBitRate(),
                              "The warping should stop after loss_exp");

        // Loss alone raises the congestion signal, through DLOSS and PLRREF.
        Ptr<NadaAlgorithmServer> lossy = Create50Mbps();
        now += step;
        Report(lossy, now, Seconds(0), recv, 0.10);
        NS_TEST_ASSERT_MSG_LT(lossy->GetTargetRate().GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "NADA should include packet loss in its congestion signal");

        // Ramp-up from a clear start follows Eqs. 3 and 4: gamma = 50 / (20 + 100 + 120).
        Ptr<NadaAlgorithmServer> ramp = Create50Mbps();
        now += step;
        Report(ramp, now, Seconds(0), DataRate("20Mbps"), 0.0);
        NS_TEST_ASSERT_MSG_EQ_TOL(static_cast<double>(ramp->GetTargetRate().GetBitRate()),
                                  20e6 * (1.0 + 50.0 / 240.0),
                                  1.0,
                                  "Unexpected accelerated ramp-up rate");

        // Delay buildup blocks ramp-up for LOGWIN, even once the delay has cleared.
        now += step;
        Report(ramp, now, MilliSeconds(15), DataRate("20Mbps"), 0.0);
        const double blockedRate = ramp->GetTargetRate().GetBitRate();
        now += step;
        Report(ramp, now, Seconds(0), DataRate("40Mbps"), 0.0);
        NS_TEST_ASSERT_MSG_LT(ramp->GetTargetRate().GetBitRate(),
                              DataRate("40Mbps").GetBitRate(),
                              "Ramp-up should wait for LOGWIN after delay buildup");
        now += MilliSeconds(500);
        Report(ramp, now, Seconds(0), DataRate("40Mbps"), 0.0);
        NS_TEST_ASSERT_MSG_EQ(ramp->GetTargetRate().GetBitRate(),
                              DataRate("48333333bps").GetBitRate(),
                              "Ramp-up should resume after LOGWIN");
        NS_TEST_ASSERT_MSG_GT(ramp->GetTargetRate().GetBitRate(),
                              blockedRate,
                              "Ramp-up should raise the rate");

        // Gradual update at a constant signal follows Eq. 7. The first report sets x_prev and
        // the second has x_diff = 0: x_offset = 30 ms - 10 ms * 50 / r.
        Ptr<NadaAlgorithmServer> gradual = Create50Mbps();
        now += step;
        Report(gradual, now, MilliSeconds(30), recv, 0.0);
        const double rate = gradual->GetTargetRate().GetBitRate();
        now += step;
        Report(gradual, now, MilliSeconds(30), recv, 0.0);
        const double offset = 0.03 - 0.01 * 50e6 / rate;
        NS_TEST_ASSERT_MSG_EQ_TOL(static_cast<double>(gradual->GetTargetRate().GetBitRate()),
                                  rate * (1.0 - 0.5 * (0.1 / 0.5) * (offset / 0.5)),
                                  1.0,
                                  "Unexpected gradual update");

        Simulator::Destroy();
    }
};

class NetXRateWaterFillingTestCase : public TestCase
{
  public:
    NetXRateWaterFillingTestCase()
        : TestCase("NetXRate water filling respects weighted demand and capacity")
    {
    }

  private:
    void DoRun() override
    {
        const auto weightedAllocations = OranLogicVrBitrate::CalculateNetXRateAllocations(
            {DataRate("100Mbps"), DataRate("100Mbps")},
            {1.0, 0.5},
            DataRate("100Mbps"));
        NS_TEST_ASSERT_MSG_EQ(weightedAllocations[0].GetBitRate(),
                              DataRate("50Mbps").GetBitRate(),
                              "First UE should receive half the cell capacity");
        NS_TEST_ASSERT_MSG_EQ(weightedAllocations[1].GetBitRate(),
                              DataRate("25Mbps").GetBitRate(),
                              "Second UE recommendation should include its capacity ratio");

        const auto demandLimitedAllocations = OranLogicVrBitrate::CalculateNetXRateAllocations(
            {DataRate("10Mbps"), DataRate("100Mbps")},
            {1.0, 1.0},
            DataRate("100Mbps"));
        NS_TEST_ASSERT_MSG_EQ(demandLimitedAllocations[0].GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "A UE below the water level should receive its full demand");
        NS_TEST_ASSERT_MSG_EQ(demandLimitedAllocations[1].GetBitRate(),
                              DataRate("90Mbps").GetBitRate(),
                              "Unused capacity should be shared with unsatisfied UEs");

        const auto unconstrainedAllocations = OranLogicVrBitrate::CalculateNetXRateAllocations(
            {DataRate("10Mbps"), DataRate("20Mbps")},
            {0.5, 1.0},
            DataRate("100Mbps"));
        NS_TEST_ASSERT_MSG_EQ(unconstrainedAllocations[0].GetBitRate(),
                              DataRate("10Mbps").GetBitRate(),
                              "NetXRate should not exceed weighted demand");
        NS_TEST_ASSERT_MSG_EQ(unconstrainedAllocations[1].GetBitRate(),
                              DataRate("20Mbps").GetBitRate(),
                              "NetXRate should preserve demand when capacity is sufficient");

        const auto zeroCapacityRatioAllocation =
            OranLogicVrBitrate::CalculateNetXRateAllocations({DataRate("5Mbps")},
                                                             {0.0},
                                                             DataRate("100Mbps"));
        NS_TEST_ASSERT_MSG_EQ(zeroCapacityRatioAllocation[0].GetBitRate(),
                              0,
                              "A UE with no effective radio capacity should receive no rate");
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
        AddTestCase(new NetXRateWaterFillingTestCase(), TestCase::Duration::QUICK);
    }
};

static GccNadaAdaptationAlgorithmTestSuite g_gccNadaAdaptationAlgorithmTestSuite;

} // namespace ns3
