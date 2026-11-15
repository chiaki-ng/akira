
#include "test_util.hpp"

#include "platform/pads/extended/ps_output.hpp"

#include <cstring>

using namespace akira::input;
using namespace akira::platform::extended;

namespace {

constexpr std::size_t kFrameEffects = 3;
constexpr std::size_t kFrameCrc     = 74;

std::uint32_t Crc32(const std::uint8_t* data, std::size_t len)
{
    return Ds5Crc32(0xffffffffu, data, len) ^ 0xffffffffu;
}

std::uint32_t FrameCrc(const std::uint8_t* frame)
{
    return (std::uint32_t)frame[kFrameCrc]
         | ((std::uint32_t)frame[kFrameCrc + 1] << 8)
         | ((std::uint32_t)frame[kFrameCrc + 2] << 16)
         | ((std::uint32_t)frame[kFrameCrc + 3] << 24);
}

} // namespace

TEST(ps_output_crc32_matches_the_standard_check_value)
{
    const char* s = "123456789";
    CHECK_EQ((int)Crc32((const std::uint8_t*)s, 9), (int)0xcbf43926u);
}

TEST(ps_output_rumble_frame_has_the_shape_the_pad_expects)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5RumbleFrame(0, 0x40, 0x80, frame, sizeof(frame)) == kDs5BluetoothFrameBytes);

    CHECK_EQ((int)frame[0], 0x31);
    CHECK_EQ((int)frame[1], 0x00);
    CHECK_EQ((int)frame[2], (int)kDs5OutputTag);

    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_RumbleLeft],  0x40);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_RumbleRight], 0x80);

    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits3] & Ds5Enable3_Rumble) != 0);

    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits1] & Ds5Enable1_HapticsSelect) != 0);

    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_LedRed], 0);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_LedGreen], 0);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_LedBlue], 0);
}

TEST(ps_output_crc_covers_the_untransmitted_header_byte)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5RumbleFrame(0, 1, 2, frame, sizeof(frame)) == kDs5BluetoothFrameBytes);

    std::uint8_t with_header[1 + kDs5BluetoothFrameBytes - 4];
    with_header[0] = 0xa2;
    std::memcpy(with_header + 1, frame, kDs5BluetoothFrameBytes - 4);

    CHECK_EQ((int)FrameCrc(frame), (int)Crc32(with_header, sizeof(with_header)));

    CHECK(FrameCrc(frame) != Crc32(frame, kDs5BluetoothFrameBytes - 4));
}

TEST(ps_output_crc_changes_with_the_amplitudes)
{
    std::uint8_t quiet[kDs5BluetoothFrameBytes];
    std::uint8_t loud[kDs5BluetoothFrameBytes];

    REQUIRE(BuildDs5RumbleFrame(0, 0, 0, quiet, sizeof(quiet)) == kDs5BluetoothFrameBytes);
    REQUIRE(BuildDs5RumbleFrame(0, 255, 255, loud, sizeof(loud)) == kDs5BluetoothFrameBytes);

    CHECK(FrameCrc(quiet) != FrameCrc(loud));
}

TEST(ps_output_refuses_a_buffer_it_cannot_fill)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];

    CHECK_EQ((int)BuildDs5RumbleFrame(0, 1, 1, frame, kDs5BluetoothFrameBytes - 1), 0);
    CHECK_EQ((int)BuildDs5BluetoothFrame(0, nullptr, 0, frame, sizeof(frame)), 0);

    std::uint8_t oversized[kDs5EffectsBytes + 1] = {};
    CHECK_EQ((int)BuildDs5BluetoothFrame(0, oversized, sizeof(oversized), frame, sizeof(frame)), 0);
}

TEST(ps_output_claims_only_pads_whose_report_we_know)
{
    CHECK(PadTakesDirectOutput(0x054c, 0x0ce6));
    CHECK(PadTakesDirectOutput(0x054c, 0x0df2));

    CHECK(!PadTakesDirectOutput(0x054c, 0x09cc));
    CHECK(!PadTakesDirectOutput(0x057e, 0x2009));
    CHECK(!PadTakesDirectOutput(0x0000, 0x0000));
}

TEST(ps_output_trigger_frame_places_each_trigger_where_the_pad_reads_it)
{
    std::uint8_t left[10];
    std::uint8_t right[10];
    for (int i = 0; i < 10; i++) {
        left[i]  = (std::uint8_t)(0xa0 + i);
        right[i] = (std::uint8_t)(0xb0 + i);
    }

    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5TriggerFrame(0, 0x21, left, 0x26, right, frame, sizeof(frame))
            == kDs5BluetoothFrameBytes);

    const std::uint8_t* l = frame + kFrameEffects + Ds5Effects_LeftTrigger;
    const std::uint8_t* r = frame + kFrameEffects + Ds5Effects_RightTrigger;

    CHECK_EQ((int)l[0], 0x21);
    CHECK_EQ((int)r[0], 0x26);

    for (int i = 0; i < 10; i++) {
        CHECK_EQ((int)l[1 + i], (int)left[i]);
        CHECK_EQ((int)r[1 + i], (int)right[i]);
    }

    CHECK((std::size_t)Ds5Effects_RightTrigger < (std::size_t)Ds5Effects_LeftTrigger);

    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits1] & Ds5Enable1_LeftTrigger) != 0);
    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits1] & Ds5Enable1_RightTrigger) != 0);
}

TEST(ps_output_trigger_frame_claims_nothing_it_does_not_set)
{
    std::uint8_t params[10] = {};
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5TriggerFrame(0, 0, params, 0, params, frame, sizeof(frame))
            == kDs5BluetoothFrameBytes);

    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits3] & Ds5Enable3_Rumble) == 0);
    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits1] & Ds5Enable1_RumbleLegacy) == 0);
}

TEST(ps_output_every_frame_states_a_vibration_mode)
{
    std::uint8_t loud[kDs5BluetoothFrameBytes];
    std::uint8_t quiet[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5RumbleFrame(0, 200, 200, loud, sizeof(loud)) == kDs5BluetoothFrameBytes);
    REQUIRE(BuildDs5RumbleFrame(0, 0, 0, quiet, sizeof(quiet)) == kDs5BluetoothFrameBytes);

    const std::uint8_t loud1  = loud[kFrameEffects + Ds5Effects_EnableBits1];
    const std::uint8_t loud3  = loud[kFrameEffects + Ds5Effects_EnableBits3];
    const std::uint8_t quiet1 = quiet[kFrameEffects + Ds5Effects_EnableBits1];
    const std::uint8_t quiet3 = quiet[kFrameEffects + Ds5Effects_EnableBits3];

    CHECK((loud1 & Ds5Enable1_HapticsSelect) != 0);
    CHECK((loud3 & Ds5Enable3_Rumble) != 0);

    CHECK((quiet1 & Ds5Enable1_HapticsSelect) == 0);
    CHECK((quiet1 & Ds5Enable1_RumbleLegacy) != 0);
    CHECK((quiet3 & Ds5Enable3_Rumble) == 0);
    CHECK((quiet3 & Ds5Enable3_RumbleNotHaptic) == 0);

    CHECK_EQ((int)quiet[kFrameEffects + Ds5Effects_RumbleLeft], 0);
    CHECK_EQ((int)quiet[kFrameEffects + Ds5Effects_RumbleRight], 0);
}

TEST(ps_output_restore_frame_is_the_transition_back_to_haptics)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5HapticsRestoreFrame(0, frame, sizeof(frame)) == kDs5BluetoothFrameBytes);

    const std::uint8_t bits1 = frame[kFrameEffects + Ds5Effects_EnableBits1];
    const std::uint8_t bits3 = frame[kFrameEffects + Ds5Effects_EnableBits3];

    CHECK((bits1 & Ds5Enable1_RumbleLegacy) != 0);
    CHECK((bits1 & Ds5Enable1_HapticsSelect) == 0);
    CHECK((bits3 & Ds5Enable3_Rumble) == 0);
    CHECK((bits3 & Ds5Enable3_RumbleNotHaptic) == 0);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_RumbleLeft], 0);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_RumbleRight], 0);
}

TEST(ps_output_rumble_frame_claims_neither_trigger)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5RumbleFrame(0, 10, 20, frame, sizeof(frame)) == kDs5BluetoothFrameBytes);

    const std::uint8_t bits = frame[kFrameEffects + Ds5Effects_EnableBits1];
    CHECK((bits & Ds5Enable1_LeftTrigger) == 0);
    CHECK((bits & Ds5Enable1_RightTrigger) == 0);
}

TEST(ps_output_haptic32_audio_section_matches_the_0x32_variant)
{
    std::int8_t samples[kDs5Haptic32SampleBytes];
    for (std::size_t i = 0; i < sizeof(samples); i++)
        samples[i] = (std::int8_t)(i - 32);

    std::uint8_t frame[kDs5Haptic32FrameBytes];
    REQUIRE(BuildDs5Haptic32Frame(0, 7, samples, sizeof(samples), frame, sizeof(frame))
            == kDs5Haptic32FrameBytes);

    CHECK_EQ((int)frame[0], 0x32);
    CHECK_EQ((int)frame[2], 0x91);
    CHECK_EQ((int)frame[3], 7);
    CHECK_EQ((int)frame[4], 0xfe);
    CHECK_EQ((int)frame[5], 0);
    CHECK_EQ((int)frame[6], 0);
    CHECK_EQ((int)frame[7], 0);
    CHECK_EQ((int)frame[8], 0);
    CHECK_EQ((int)frame[9], 0xff);
    CHECK_EQ((int)frame[10], 7);
    CHECK_EQ((int)frame[11], 0x92);
    CHECK_EQ((int)frame[12], (int)kDs5Haptic32SampleBytes);
    CHECK(std::memcmp(frame + 13, samples, sizeof(samples)) == 0);
}

TEST(ps_output_haptic_frame_header_matches_the_frame_that_moved_the_coils)
{
    std::int8_t samples[kDs5HapticBlockBytes * 2]{};
    std::uint8_t f[kDs5HapticFrameBytes];

    REQUIRE(BuildDs5HapticFrame(0, 0, samples, sizeof(samples), f, sizeof(f))
            == kDs5HapticFrameBytes);

    CHECK_EQ((int)f[0],  0x39);
    CHECK_EQ((int)f[2],  0x91);
    CHECK_EQ((int)f[3],  6);
    CHECK_EQ((int)f[4],  0x7e);
    CHECK_EQ((int)f[5],  48);
    CHECK_EQ((int)f[6],  48);
    CHECK_EQ((int)f[7],  48);
    CHECK_EQ((int)f[8],  48);
    CHECK_EQ((int)f[10], 0xd2);
    CHECK_EQ((int)f[11], 64);
}

TEST(ps_output_haptic_block_count_bit_tracks_the_payload)
{
    std::int8_t one[kDs5Haptic32SampleBytes]{};
    std::int8_t two[kDs5HapticBlockBytes * 2]{};

    std::uint8_t small[kDs5Haptic32FrameBytes];
    std::uint8_t large[kDs5HapticFrameBytes];

    REQUIRE(BuildDs5Haptic32Frame(0, 0, one, sizeof(one), small, sizeof(small))
            == kDs5Haptic32FrameBytes);
    REQUIRE(BuildDs5HapticFrame(0, 0, two, sizeof(two), large, sizeof(large))
            == kDs5HapticFrameBytes);

    CHECK((small[11] & 0x40) == 0);
    CHECK((large[10] & 0x40) != 0);
}

TEST(ps_output_builders_leave_the_sequence_byte_to_the_sender)
{
    std::int8_t samples[kDs5Haptic32SampleBytes]{};

    std::uint8_t haptic[kDs5Haptic32FrameBytes];
    std::uint8_t rumble[kDs5BluetoothFrameBytes];

    REQUIRE(BuildDs5Haptic32Frame(0, 0, samples, sizeof(samples), haptic, sizeof(haptic))
            == kDs5Haptic32FrameBytes);
    REQUIRE(BuildDs5RumbleFrame(0, 0, 0, rumble, sizeof(rumble)) == kDs5BluetoothFrameBytes);

    CHECK_EQ((int)haptic[1], 0);
    CHECK_EQ((int)rumble[1], 0);
}

TEST(ps_output_stamping_a_sequence_requires_restamping_the_crc)
{
    std::int8_t samples[kDs5Haptic32SampleBytes]{};

    std::uint8_t frame[kDs5Haptic32FrameBytes];
    REQUIRE(BuildDs5Haptic32Frame(0, 0, samples, sizeof(samples), frame, sizeof(frame))
            == kDs5Haptic32FrameBytes);

    const auto crc_of = [](const std::uint8_t* f, std::size_t len) {
        const std::uint8_t header = 0xa2;
        std::uint32_t c = Ds5Crc32(0xffffffffu, &header, 1);
        return Ds5Crc32(c, f, len - 4) ^ 0xffffffffu;
    };
    const auto stored = [](const std::uint8_t* f, std::size_t len) {
        return (std::uint32_t)f[len - 4]
             | ((std::uint32_t)f[len - 3] << 8)
             | ((std::uint32_t)f[len - 2] << 16)
             | ((std::uint32_t)f[len - 1] << 24);
    };

    CHECK_EQ((int)stored(frame, sizeof(frame)), (int)crc_of(frame, sizeof(frame)));

    frame[1] = 0x50;
    CHECK((int)stored(frame, sizeof(frame)) != (int)crc_of(frame, sizeof(frame)));

    StampDs5Crc(frame, sizeof(frame));
    CHECK_EQ((int)stored(frame, sizeof(frame)), (int)crc_of(frame, sizeof(frame)));
}

TEST(ps_output_intensity_packs_triggers_high_and_vibration_low)
{
    CHECK_EQ((int)Ds5IntensityByte(Ds5EffectIntensity::Strong, Ds5EffectIntensity::Strong), 0x00);
    CHECK_EQ((int)Ds5IntensityByte(Ds5EffectIntensity::Strong, Ds5EffectIntensity::Off),    0xf0);
    CHECK_EQ((int)Ds5IntensityByte(Ds5EffectIntensity::Off,    Ds5EffectIntensity::Strong), 0x0f);
    CHECK_EQ((int)Ds5IntensityByte(Ds5EffectIntensity::Weak,   Ds5EffectIntensity::Medium), 0x63);
    CHECK_EQ((int)Ds5IntensityByte(Ds5EffectIntensity::Medium, Ds5EffectIntensity::Weak),   0x92);
}

TEST(ps_output_intensity_counts_down_from_full)
{
    const int strong = Ds5IntensityByte(Ds5EffectIntensity::Strong, Ds5EffectIntensity::Strong);
    const int medium = Ds5IntensityByte(Ds5EffectIntensity::Medium, Ds5EffectIntensity::Medium);
    const int weak   = Ds5IntensityByte(Ds5EffectIntensity::Weak,   Ds5EffectIntensity::Weak);

    CHECK_EQ(strong, (int)kDs5IntensityFull);
    CHECK((medium & 0x0f) > (strong & 0x0f));
    CHECK((weak   & 0x0f) > (medium & 0x0f));
    CHECK((medium & 0xf0) > (strong & 0xf0));
    CHECK((weak   & 0xf0) > (medium & 0xf0));
}

TEST(ps_output_every_frame_claims_the_intensity_byte)
{
    std::uint8_t params[10] = {};
    const std::uint8_t want = Ds5IntensityByte(Ds5EffectIntensity::Weak,
                                               Ds5EffectIntensity::Medium);

    std::uint8_t rumble[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5RumbleFrame(0, 128, 128, rumble, sizeof(rumble), false,
                                Ds5FrameLayout::Tagged, want) == kDs5BluetoothFrameBytes);

    std::uint8_t trigger[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5TriggerFrame(0, 0x21, params, 0x21, params, trigger, sizeof(trigger), want)
            == kDs5BluetoothFrameBytes);

    std::uint8_t state[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5StateFrame(0, 128, 128, 0x21, params, 0x21, params, state, sizeof(state),
                               false, Ds5FrameLayout::Tagged, want) == kDs5BluetoothFrameBytes);

    const std::uint8_t* frames[] = { rumble, trigger, state };
    for (const std::uint8_t* f : frames) {
        CHECK((f[kFrameEffects + Ds5Effects_EnableBits2] & Ds5Enable2_Intensity) != 0);
        CHECK_EQ((int)f[kFrameEffects + Ds5Effects_Intensity], (int)want);
    }
}

TEST(ps_output_handing_the_pad_back_states_no_preference)
{
    std::uint8_t frame[kDs5BluetoothFrameBytes];
    REQUIRE(BuildDs5HapticsRestoreFrame(0, frame, sizeof(frame)) == kDs5BluetoothFrameBytes);

    CHECK((frame[kFrameEffects + Ds5Effects_EnableBits2] & Ds5Enable2_Intensity) != 0);
    CHECK_EQ((int)frame[kFrameEffects + Ds5Effects_Intensity], (int)kDs5IntensityFull);
}
