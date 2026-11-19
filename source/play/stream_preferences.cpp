#include "play/stream_preferences.hpp"
#include "log/log.hpp"

#include <algorithm>

#include "cloud/cloud_launch.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/host.hpp"
#include "network/network_settings.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/stream_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/takion_target.hpp"

namespace akira::play {

namespace play = libchiaking::play;

StreamPreferences::StreamPreferences(const StreamSettings& stream, const cloud::CloudSettings& cloud,
                                     const network::NetworkSettings& network, const settings::DeveloperSettings& developer,
                                     const akira::platform::PlatformFactory& platform,
                                     network::WireGuardManager& wireGuard)
    : streamSettings(stream), cloudSettings(cloud), networkSettings(network), developerSettings(developer),
      platformFactory(platform), wireGuard(wireGuard)
{
}

auto StreamPreferences::forConsole(consoles::Host& host) const -> std::optional<play::Preferences>
{
    const bool vpn = !host.isRemote() && wireGuard.isConnected();
    const auto& stream = streamSettings;
    const auto profile = vpn ? StreamProfile::Vpn : host.isRemote() ? StreamProfile::Remote : StreamProfile::Local;
    uint32_t height = static_cast<uint32_t>(akira::play::toInt(stream.videoResolution(profile)));
    const auto fps = static_cast<uint32_t>(akira::play::toInt(stream.videoFps(profile)));
    const auto bitrate = static_cast<uint32_t>(std::clamp(stream.videoBitrate(profile),
        static_cast<int>(play::PictureRequest::minimum_bitrate_kbps),
        static_cast<int>(play::PictureRequest::maximum_bitrate_kbps)));
    const auto codec = host.isPS5() && platformFactory.supplies(akira::platform::PlatformCapability::hevc)
        ? libchiaking::media::VideoCodec::h265 : libchiaking::media::VideoCodec::h264;
    if (codec == libchiaking::media::VideoCodec::h264)
        height = std::min(height, play::PictureRequest::maximum_h264_height);

    const auto picture = play::PictureRequest::create(height, fps, bitrate, codec);
    if (!picture)
    {
        log::error("Stream preferences refused: {}p {} fps {} kbps", height, fps, bitrate);
        return std::nullopt;
    }

    const auto target = host.isPS5() ? settings::TakionTarget::PS5 : settings::TakionTarget::PS4;
    const auto takionVersion = host.getPinnedTakionVersion().value_or(
        static_cast<uint32_t>(std::max(developerSettings.takionVersion(target), 0)));
    if (host.getPinnedTakionVersion())
        log::info("{} is pinned to Takion v{}", host.getHostName(), takionVersion);
    auto preferences = tuned(*picture, takionVersion);
    if (preferences && vpn)
    {
        if (auto network = host.homeNetwork(wireGuard); network.network())
            preferences->home = std::move(network);
        else
            log::error("Stream over WireGuard has no tunnel network; using the local network");
    }
    return preferences;
}

auto StreamPreferences::forCloud(const cloud::CloudLaunch& launch) const -> std::optional<play::Preferences>
{
    const bool ps5Cloud = launch.ps5();
    const auto codec = ps5Cloud && platformFactory.supplies(akira::platform::PlatformCapability::hevc)
        ? libchiaking::media::VideoCodec::h265 : libchiaking::media::VideoCodec::h264;
    auto height = static_cast<uint32_t>(cloudSettings.videoResolution(ps5Cloud));
    if (codec == libchiaking::media::VideoCodec::h264)
        height = std::min(height, play::PictureRequest::maximum_h264_height);
    const auto bitrate = static_cast<uint32_t>(std::clamp(cloudSettings.videoBitrate(ps5Cloud),
        static_cast<int>(play::PictureRequest::minimum_bitrate_kbps),
        static_cast<int>(play::PictureRequest::maximum_bitrate_kbps)));
    const auto picture = play::PictureRequest::create(height, 60, bitrate, codec);
    if (!picture)
    {
        log::error("Cloud stream preferences refused: {}p {} kbps", height, bitrate);
        return std::nullopt;
    }
    return tuned(*picture,
        static_cast<uint32_t>(std::max(developerSettings.takionVersion(ps5Cloud ? settings::TakionTarget::PS5Cloud : settings::TakionTarget::PSNow), 0)));
}

auto StreamPreferences::tuned(play::PictureRequest picture, std::uint32_t takionVersion) const
    -> std::optional<play::Preferences>
{
    play::DeveloperTuning::Facts facts;
    if (takionVersion > 0)
        facts.protocol_version = takionVersion;
    facts.packet_loss_max = streamSettings.packetLossMax();
    facts.keyframe_on_lost_frame = streamSettings.requestIdrOnFecFailure();
    facts.nat_probe_sockets = static_cast<std::size_t>(std::clamp(networkSettings.portGuessingSocks(), 0,
        static_cast<int>(play::DeveloperTuning::maximum_nat_probe_sockets)));
    const auto developer = play::DeveloperTuning::create(facts);
    const auto internet = play::InternetOptions::create(networkSettings.portGuessing(),
        static_cast<std::size_t>(std::max(networkSettings.portGuessingCount(), 0)));
    if (!developer || !internet)
    {
        log::error("Stream tuning refused: packet loss {} sockets {}", facts.packet_loss_max, facts.nat_probe_sockets);
        return std::nullopt;
    }

    play::Preferences preferences;
    preferences.picture = picture;
    preferences.sound = play::SoundRequest::create(play::SoundRequest::stereo)
                            .and_then([](const play::SoundRequest& sound) { return sound.capped_at(play::SoundRequest::stereo); })
                            .transform([](const play::SoundRequest& sound) { return sound.with_audio_profiles(false); })
                            .value_or(play::SoundRequest{});
    preferences.developer = *developer;
    preferences.internet = *internet;
    return preferences;
}

}
