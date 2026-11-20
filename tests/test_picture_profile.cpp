#include "test_util.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>

#include "play/picture_profile.hpp"
#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"

using akira::play::PictureProfile;
using akira::play::StreamProfile;
using akira::play::StreamSettings;
using akira::play::VideoFps;
using akira::play::VideoResolution;

namespace {

class PictureFixture {
public:
    explicit PictureFixture(bool fsr4k)
        : root(std::filesystem::temp_directory_path()
              / std::format("akira-picture-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(),
                  counter++))
        , store(root)
        , stream(store, fsr4k)
    {
        store.finishLoading();
    }

    ~PictureFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    PictureFixture(const PictureFixture&) = delete;
    PictureFixture& operator=(const PictureFixture&) = delete;

    static inline std::atomic<int> counter{ 0 };
    std::filesystem::path root;
    akira::settings::SettingsStore store;
    StreamSettings stream;
};

}

TEST(picture_profile_reads_the_native_profile)
{
    PictureFixture fixture(false);
    fixture.stream.setLocalVideoResolution(VideoResolution::P1080);
    fixture.stream.setLocalVideoBitrate(15000);
    const PictureProfile profile(fixture.stream, StreamProfile::Local, false);
    CHECK(profile.resolution() == VideoResolution::P1080);
    CHECK(!profile.upscaling());
    CHECK(profile.bitrate() == 15000);
}

TEST(picture_profile_upscale_picks_the_fsr_source)
{
    PictureFixture fixture(false);
    PictureProfile profile(fixture.stream, StreamProfile::Remote, false);
    profile.chooseUpscale(1080);
    CHECK(profile.upscaling());
    CHECK(profile.resolution() == VideoResolution::P720);
    CHECK(profile.bitrate() == fixture.stream.defaultBitrate(VideoResolution::P720));
    profile.applyTo(fixture.stream);
    CHECK(fixture.stream.remoteFsrEnabled());
    CHECK(fixture.stream.fsrTarget(StreamProfile::Remote) == 1080);
    CHECK(fixture.stream.remoteVideoResolution() == VideoResolution::P720);
    const PictureProfile reread(fixture.stream, StreamProfile::Remote, false);
    CHECK(reread == profile);
}

TEST(picture_profile_offers_4k_targets_only_with_fsr_4k)
{
    PictureFixture nx(false);
    PictureProfile small(nx.stream, StreamProfile::Local, false);
    CHECK(small.upscaleTargets().size() == 2);
    small.chooseUpscale(2160);
    CHECK(small.upscaleTarget() == 1080);

    PictureFixture ounce(true);
    PictureProfile large(ounce.stream, StreamProfile::Local, true);
    CHECK(large.upscaleTargets().size() == 4);
    large.chooseUpscale(2160);
    CHECK(large.upscaleTarget() == 2160);
    CHECK(large.resolution() == VideoResolution::P1080);
}

TEST(picture_profile_turning_upscale_off_keeps_the_source)
{
    PictureFixture fixture(false);
    PictureProfile profile(fixture.stream, StreamProfile::Local, false);
    profile.chooseUpscale(720);
    profile.chooseUpscale(0);
    CHECK(!profile.upscaling());
    CHECK(profile.resolution() == VideoResolution::P540);
    profile.applyTo(fixture.stream);
    CHECK(!fixture.stream.localFsrEnabled());
    CHECK(fixture.stream.fsrTarget(StreamProfile::Local) == 0);
}

TEST(picture_profile_bitrate_snaps_and_clamps)
{
    PictureFixture fixture(false);
    PictureProfile profile(fixture.stream, StreamProfile::Local, false);
    profile.chooseResolution(VideoResolution::P720);
    profile.chooseBitrate(12340);
    CHECK(profile.bitrate() == 12500);
    profile.chooseBitrate(999999);
    CHECK(profile.bitrate() == profile.maxBitrate());
    profile.chooseBitrate(1);
    CHECK(profile.bitrate() == profile.minBitrate());
    profile.chooseFps(VideoFps::Fps30);
    profile.applyTo(fixture.stream);
    CHECK(fixture.stream.localVideoFps() == VideoFps::Fps30);
    CHECK(fixture.stream.localVideoBitrate() == profile.minBitrate());
}

TEST(stream_settings_rcas_strength_round_trips_the_sharpness)
{
    PictureFixture fixture(false);
    fixture.stream.setRcasStrength(90);
    CHECK(fixture.stream.rcasStrength() == 90);
    CHECK(fixture.stream.rcasSharpness() > 0.19f && fixture.stream.rcasSharpness() < 0.21f);
    fixture.stream.setRcasStrength(150);
    CHECK(fixture.stream.rcasStrength() == 100);
}
