#include "test_util.hpp"

#include "play/gesture_timing.hpp"

using namespace std::chrono_literals;

TEST(gesture_durations_preserve_legacy_60hz_behavior)
{
    using namespace akira::input::gesture;

    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(SwipeDuration).count(), 300);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(BorderTapCommitDelay).count(), 66);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(TouchpadButtonDelay).count(), 66);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(TouchpadButtonHold).count(), 200);
    CHECK_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(TouchWarningInterval).count(), 5000);
}

TEST(gesture_interpolation_uses_elapsed_progress)
{
    using akira::input::gesture::interpolate;

    CHECK_EQ(interpolate(0, 1920, 0ms, 300ms), 0);
    CHECK_EQ(interpolate(0, 1920, 150ms, 300ms), 960);
    CHECK_EQ(interpolate(0, 1920, 300ms, 300ms), 1920);
    CHECK_EQ(interpolate(1920, 0, 75ms, 300ms), 1440);
    CHECK_EQ(interpolate(1920, 0, 400ms, 300ms), 0);
}
