#include "test_util.hpp"

#include "input/controller_timing.hpp"

using namespace std::chrono_literals;

TEST(controller_lifecycle_durations_preserve_60hz_behavior)
{
    using namespace akira::input::timing;

    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(ArrivalPollPeriod).count(), 500);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(CouchClaimHold).count(), 666);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(DisconnectGrace).count(), 500);
}

TEST(controller_claim_progress_uses_elapsed_time)
{
    using namespace akira::input::timing;
    const auto start = TimePoint{} + 1s;

    CHECK_EQ(progress(start, start - 1ms, CouchClaimHold), 0.0f);
    CHECK(progress(start, start + 333ms, CouchClaimHold) > 0.49f);
    CHECK(progress(start, start + 333ms, CouchClaimHold) < 0.51f);
    CHECK_EQ(progress(start, start + 1s, CouchClaimHold), 1.0f);
}
