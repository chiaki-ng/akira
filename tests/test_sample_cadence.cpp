#include "test_util.hpp"

#include "input/sample_cadence.hpp"

using namespace std::chrono_literals;

TEST(sample_cadence_is_due_immediately_then_at_its_period)
{
    akira::input::SampleCadence cadence(16ms);
    const auto start = akira::input::SampleCadence::TimePoint{} + 1s;

    CHECK(cadence.due(start));
    CHECK(!cadence.due(start + 8ms));
    CHECK(cadence.due(start + 16ms));
    CHECK(!cadence.due(start + 24ms));
    CHECK(cadence.due(start + 32ms));
}

TEST(sample_cadence_resynchronizes_after_a_late_caller)
{
    akira::input::SampleCadence cadence(16ms);
    const auto start = akira::input::SampleCadence::TimePoint{} + 1s;

    CHECK(cadence.due(start));
    CHECK(cadence.due(start + 40ms));
    CHECK(!cadence.due(start + 48ms));
    CHECK(cadence.due(start + 56ms));
}

TEST(sample_cadence_gates_120hz_calls_to_60hz)
{
    akira::input::SampleCadence cadence(akira::input::MotionPollPeriod);
    const auto start = akira::input::SampleCadence::TimePoint{} + 1s;

    int motionSamples = 0;
    for (int inputSample = 0; inputSample < 120; inputSample++) {
        if (cadence.due(start + akira::input::InputPollPeriod * inputSample))
            motionSamples++;
    }

    CHECK_EQ(motionSamples, 60);
}

TEST(sample_cadence_emits_only_once_after_a_long_stall)
{
    akira::input::SampleCadence cadence(16ms);
    const auto start = akira::input::SampleCadence::TimePoint{} + 1s;

    CHECK(cadence.due(start));
    CHECK(cadence.due(start + 100ms));
    CHECK(!cadence.due(start + 101ms));
    CHECK(cadence.due(start + 116ms));
}

TEST(sample_cadence_limits_direct_device_discovery_to_twice_per_second)
{
    akira::input::SampleCadence cadence(akira::input::DirectDiscoveryPeriod);
    const auto start = akira::input::SampleCadence::TimePoint{} + 1s;

    int attempts = 0;
    for (int inputSample = 0; inputSample < 120; inputSample++) {
        if (cadence.due(start + akira::input::InputPollPeriod * inputSample))
            attempts++;
    }

    CHECK_EQ(attempts, 2);
}
