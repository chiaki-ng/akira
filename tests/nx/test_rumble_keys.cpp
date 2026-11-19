#include "test_util.hpp"

#include "play/rumble_profile.hpp"
#include "platform/pads/extended/pad_output_state.hpp"
#include "platform/pads/extended/rumble_keys.hpp"

#include <vector>

using namespace akira::play;
using namespace akira::platform::extended;

namespace {

std::vector<std::int16_t> Stereo(std::int16_t left, std::int16_t right, std::size_t frames = 128)
{
    std::vector<std::int16_t> buf(frames * 2);
    for (std::size_t i = 0; i < frames; i++) {
        buf[i * 2]     = left;
        buf[i * 2 + 1] = right;
    }
    return buf;
}

} // namespace

TEST(rumble_haptic_reaches_full_scale)
{
    auto buf = Stereo(32767, 32767);
    const HapticRumble r = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Normal);

    CHECK(r.emit);
    CHECK_EQ((int)(r.left >> 8), 255);
    CHECK_EQ((int)(r.right >> 8), 255);
}

TEST(rumble_haptic_follows_the_crest_not_the_average)
{
    std::vector<std::int16_t> buf(128 * 2, 0);
    buf[0] = 32767;
    buf[1] = 32767;

    const HapticRumble r = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Normal);

    REQUIRE(r.emit);
    CHECK_EQ((int)(r.left  >> 8), 255);
    CHECK_EQ((int)(r.right >> 8), 255);
}

TEST(rumble_haptic_keeps_the_two_channels_apart)
{
    auto buf = Stereo(32767, 4000);
    const HapticRumble r = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Normal);

    CHECK(r.emit);
    CHECK(r.left > r.right);
}

TEST(rumble_haptic_floor_is_only_for_a_pad_with_a_motor_to_stall)
{
    auto faint = Stereo(80, 80);

    const HapticRumble erm = HapticAudioToRumble(faint.data(), 128,
                                                 HapticIntensity::Normal, true);
    const HapticRumble coils = HapticAudioToRumble(faint.data(), 128,
                                                   HapticIntensity::Normal, false);

    CHECK(erm.emit);
    CHECK(coils.emit);
    CHECK_EQ((int)erm.left, (int)kHapticMotorFloor);
    CHECK(coils.left < kHapticMotorFloor);
    CHECK_EQ((int)coils.left, (int)coils.raw_left);
}

TEST(rumble_haptic_gates_room_tone)
{
    auto quiet = Stereo(20, 20);
    CHECK(!HapticAudioToRumble(quiet.data(), 128, HapticIntensity::Normal).emit);

    auto loud = Stereo(4000, 4000);
    CHECK(HapticAudioToRumble(loud.data(), 128, HapticIntensity::Normal).emit);
}

TEST(rumble_haptic_says_nothing_rather_than_zero)
{
    auto quiet = Stereo(0, 0);
    const HapticRumble r = HapticAudioToRumble(quiet.data(), 128, HapticIntensity::Normal);

    CHECK(!r.emit);
    CHECK_EQ((int)r.left, 0);
    CHECK_EQ((int)r.right, 0);
}

TEST(rumble_haptic_lifts_anything_audible_over_the_stall_threshold)
{
    auto buf = Stereo(60, 60);
    const HapticRumble r = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Normal);

    REQUIRE(r.emit);
    CHECK(r.left  >= kHapticMotorFloor);
    CHECK(r.right >= kHapticMotorFloor);
}

TEST(rumble_haptic_floor_clears_the_envelope_silence_threshold)
{
    CHECK((float)kHapticMotorFloor / 65535.0f > 2.0f / 255.0f);
}

TEST(rumble_haptic_intensity_scales_both_ways)
{
    auto buf = Stereo(4000, 4000);

    const HapticRumble weak   = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Weak);
    const HapticRumble normal = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Normal);
    const HapticRumble strong = HapticAudioToRumble(buf.data(), 128, HapticIntensity::Strong);

    CHECK(weak.left < normal.left);
    CHECK(strong.left > normal.left);
}

TEST(rumble_haptic_off_emits_nothing_however_loud)
{
    auto buf = Stereo(32767, 32767);
    CHECK(!HapticAudioToRumble(buf.data(), 128, HapticIntensity::Off).emit);
}

TEST(rumble_haptic_saturates_rather_than_wrapping)
{
    auto buf = Stereo(32767, 32767);
    const HapticRumble r = HapticAudioToRumble(buf.data(), 128, HapticIntensity::VeryStrong);

    CHECK_EQ((int)r.left, 0xffff);
    CHECK_EQ((int)r.right, 0xffff);
}

TEST(rumble_profile_keys_never_collide_between_tiers)
{
    const std::string model = RumbleKeyForModel(0x054c, 0x0ce6);
    CHECK_EQ(model, std::string("054c:0ce6"));

    const std::uint8_t mac[6] = { 0x24, 0xa6, 0xfa, 0xa1, 0xf1, 0x11 };
    const std::string unit = RumbleKeyForUnit(mac);
    CHECK_EQ(unit, std::string("24:a6:fa:a1:f1:11"));

    CHECK(RumbleKeyIsModel(model));
    CHECK(!RumbleKeyIsUnit(model));
    CHECK(RumbleKeyIsUnit(unit));
    CHECK(!RumbleKeyIsModel(unit));

    CHECK(!RumbleKeyIsUnit(kRumbleKeySwitch));
    CHECK(!RumbleKeyIsModel(kRumbleKeySwitch));
    CHECK(!RumbleKeyIsUnit(kRumbleKeyDefault));
    CHECK(!RumbleKeyIsModel(kRumbleKeyDefault));
}

TEST(rumble_profile_category_keys_are_their_own_tier)
{
    for (const char* key : { kRumbleKeyDualSense, kRumbleKeyJoyCon,
                             kRumbleKeySwitchPro, kRumbleKeyGeneric }) {
        CHECK(RumbleKeyIsCategory(key));
        CHECK(!RumbleKeyIsUnit(key));
        CHECK(!RumbleKeyIsModel(key));
    }

    CHECK(!RumbleKeyIsCategory(kRumbleKeyDefault));
    CHECK(!RumbleKeyIsCategory(kRumbleKeySwitch));
    CHECK(!RumbleKeyIsCategory("054c:0ce6"));
    CHECK(!RumbleKeyIsCategory("24:a6:fa:a1:f1:11"));
}

TEST(rumble_profile_category_covers_both_dualsense_models)
{
    CHECK(PadCategoryFor(0x054c, 0x0ce6, false, false) == PadCategory::DualSense);
    CHECK(PadCategoryFor(0x054c, 0x0df2, false, false) == PadCategory::DualSense);

    CHECK(PadCategoryFor(0x054c, 0x09cc, false, false) == PadCategory::Generic);
    CHECK(PadCategoryFor(0x0000, 0x0000, false, false) == PadCategory::Generic);
}

TEST(rumble_profile_switch_pads_split_by_style_not_by_id)
{
    CHECK(PadCategoryFor(0, 0, true, true)  == PadCategory::JoyCon);
    CHECK(PadCategoryFor(0, 0, true, false) == PadCategory::SwitchPro);

    CHECK(PadCategoryFor(0x054c, 0x0ce6, true, true) == PadCategory::JoyCon);
}

TEST(rumble_profile_defaults_to_full_output_and_is_gated_by_identity)
{
    const RumbleProfile d = DefaultRumbleProfile();
    CHECK(d.output_mode == PadOutputMode::Native);

    CHECK(d.rumble_source == RumbleSource::Derived);

    CHECK(PadTakesDirectOutput(0x054c, 0x0ce6));
    CHECK(!PadTakesDirectOutput(0x054c, 0x09cc));
    CHECK(!PadTakesDirectOutput(0x0000, 0x0000));
}

static PadOutputInputs DrivingInputs()
{
    PadOutputInputs in;
    in.supported_pad    = true;
    in.profile_native   = true;
    in.backend_enabled  = true;
    in.owns_output      = true;
    in.wanted           = true;
    in.address_valid    = true;
    in.haptics_wanted   = true;
    in.haptics_landing  = true;
    return in;
}

TEST(pad_output_drives_when_everything_agrees)
{
    const PadOutputState s = ResolvePadOutput(DrivingInputs());
    CHECK(s.driver == PadDriver::Akira);
    CHECK(s.reason == PadDriverReason::Native);
    CHECK(s.want_claim);
    CHECK(s.write_state);
    CHECK(s.stream_haptics);
}

TEST(pad_output_released_beats_refused)
{
    PadOutputInputs in = DrivingInputs();
    in.backend_enabled   = false;
    in.ownership_refused = true;

    const PadOutputState s = ResolvePadOutput(in);
    CHECK(s.driver == PadDriver::MissionControl);
    CHECK(s.reason == PadDriverReason::Released);
}

TEST(pad_output_keeps_asking_after_a_refusal)
{
    PadOutputInputs in = DrivingInputs();
    in.ownership_refused = true;
    in.owns_output       = false;

    const PadOutputState s = ResolvePadOutput(in);
    CHECK(s.driver == PadDriver::MissionControl);
    CHECK(s.want_claim);
    CHECK(!s.write_state);
    CHECK(!s.stream_haptics);
}

TEST(pad_output_never_writes_to_a_pad_it_does_not_own)
{
    PadOutputInputs in = DrivingInputs();
    in.owns_output = false;

    const PadOutputState s = ResolvePadOutput(in);
    CHECK(s.want_claim);
    CHECK(!s.write_state);
    CHECK(!s.stream_haptics);
}

TEST(pad_output_separates_haptics_from_writing)
{
    PadOutputInputs in = DrivingInputs();
    in.haptics_wanted = false;

    const PadOutputState s = ResolvePadOutput(in);
    CHECK(s.driver == PadDriver::Akira);
    CHECK(s.write_state);
    CHECK(!s.stream_haptics);
}

TEST(pad_output_basic_is_not_reported_as_a_backend_problem)
{
    PadOutputInputs in = DrivingInputs();
    in.profile_native = false;

    const PadOutputState s = ResolvePadOutput(in);
    CHECK(s.driver == PadDriver::MissionControl);
    CHECK(s.reason == PadDriverReason::Basic);
    CHECK(!s.want_claim);
}
