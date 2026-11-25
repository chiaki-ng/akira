#include "play/stream_settings.hpp"
#include "settings/settings_store.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace akira::play {

StreamSettings::StreamSettings(settings::SettingsStore& store, bool fsr4k)
    : store(store), fsr4k(fsr4k)
{
    read(store.loaded());
    store.attach(*this);
}

StreamSettings::~StreamSettings()
{
    store.detach(*this);
}

void StreamSettings::read(const toml::table& config)
{
    const auto video = config["video"];
    constexpr std::array<const char*, 3> keys{"local", "remote", "vpn"};
    for (size_t i = 0; i < keys.size(); ++i) {
        const auto profile = video[keys[i]];
        Video& target = videos[i];
        if (auto val = profile["resolution"].value<std::string>())
            target.resolution = resolutionFromString(*val);
        if (auto val = profile["fps"].value<int64_t>())
            target.fps = fpsFromString(std::to_string(*val));
        if (auto val = profile["bitrate"].value<int64_t>())
            target.bitrate = static_cast<int>(*val);
        else if (i < 2)
            target.bitrate = defaultBitrate(target.resolution);
        if (auto val = profile["fsr_enabled"].value<bool>())
            target.fsr = *val;
        target.fsrTarget = static_cast<int>(profile["fsr_target"].value_or(int64_t{0}));
    }
    zeroCopy = video["zero_copy"].value_or(false);
    softwareHevcDecode = video["software_hevc"].value_or(false);
    renderer = video["renderer"].value_or(std::string("drako3d")) == "vulkan"
        ? RendererKind::Vulkan
        : RendererKind::Drako3d;

    const auto picture = config["picture"];
    if (auto val = picture["dithering_enabled"].value<bool>())
        dithering = *val;
    if (auto val = picture["dithering_strength"].value<double>())
        ditherStrength = std::max(1.0f, std::min(10.0f, static_cast<float>(*val)));
    if (auto val = picture["rcas_enabled"].value<bool>())
        rcas = *val;
    if (auto val = picture["rcas_sharpness"].value<double>())
        sharpness = static_cast<float>(*val);
    adjustments = ColourAdjustments(picture["colour_brightness"].value_or(0.0), picture["colour_contrast"].value_or(1.0),
        picture["colour_saturation"].value_or(1.0), picture["colour_gamma"].value_or(1.0));
    streamVolume = Volume(config["audio"]["volume"].value_or(Volume::highest));

    if (auto val = config["overlay"]["stats_x"].value<double>())
        overlayX = static_cast<float>(*val);
    if (auto val = config["overlay"]["stats_y"].value<double>())
        overlayY = static_cast<float>(*val);
    const auto overlay = config["overlay"];
    appearance.level = parsed(DiagnosticsAppearance::levels, &StreamSettings::levelName,
        overlay["diagnostics_level"].value_or(std::string_view()), appearance.level);
    appearance.glass = parsed(DiagnosticsAppearance::glasses, &StreamSettings::glassName,
        overlay["diagnostics_glass"].value_or(std::string_view()), appearance.glass);
    appearance.layout = parsed(DiagnosticsAppearance::layouts, &StreamSettings::layoutName,
        overlay["diagnostics_layout"].value_or(std::string_view()), appearance.layout);

    if (auto val = config["stream"]["request_idr_on_fec_failure"].value<bool>())
        idrOnFecFailure = *val;
    if (auto val = config["stream"]["packet_loss_max"].value<double>())
        packetLoss = static_cast<float>(*val);

    if (auto val = config["debug"]["unlock_bitrate_max"].value<bool>())
        bitrateUnlocked = *val;
}

void StreamSettings::write(toml::table& config) const
{
    toml::table& video = settings::SettingsStore::table(config, "video");
    constexpr std::array<const char*, 3> keys{"local", "remote", "vpn"};
    for (size_t i = 0; i < keys.size(); ++i) {
        const Video& source = videos[i];
        toml::table profile;
        profile.insert("resolution", toString(source.resolution));
        profile.insert("fps", toInt(source.fps));
        profile.insert("bitrate", source.bitrate);
        profile.insert("fsr_enabled", source.fsr);
        profile.insert("fsr_target", source.fsrTarget);
        video.insert(keys[i], std::move(profile));
    }
    video.insert("zero_copy", zeroCopy);
    video.insert("software_hevc", softwareHevcDecode);
    video.insert("renderer", renderer == RendererKind::Drako3d ? "drako3d" : "vulkan");

    toml::table picture;
    picture.insert("dithering_enabled", dithering);
    picture.insert("dithering_strength", static_cast<double>(ditherStrength));
    picture.insert("rcas_enabled", rcas);
    picture.insert("rcas_sharpness", static_cast<double>(sharpness));
    if (!adjustments.isNeutral()) {
        picture.insert("colour_brightness", adjustments.value(ColourAdjustments::Channel::Brightness));
        picture.insert("colour_contrast", adjustments.value(ColourAdjustments::Channel::Contrast));
        picture.insert("colour_saturation", adjustments.value(ColourAdjustments::Channel::Saturation));
        picture.insert("colour_gamma", adjustments.value(ColourAdjustments::Channel::Gamma));
    }
    config.insert("picture", std::move(picture));
    if (streamVolume != Volume())
        settings::SettingsStore::table(config, "audio").insert("volume", streamVolume.level());

    toml::table overlay;
    if (overlayX >= 0.0f && overlayY >= 0.0f) {
        overlay.insert("stats_x", static_cast<double>(overlayX));
        overlay.insert("stats_y", static_cast<double>(overlayY));
    }
    if (appearance != DiagnosticsAppearance{}) {
        overlay.insert("diagnostics_level", levelName(appearance.level));
        overlay.insert("diagnostics_glass", glassName(appearance.glass));
        overlay.insert("diagnostics_layout", layoutName(appearance.layout));
    }
    if (!overlay.empty())
        config.insert("overlay", std::move(overlay));

    toml::table& stream = settings::SettingsStore::table(config, "stream");
    stream.insert("request_idr_on_fec_failure", idrOnFecFailure);
    stream.insert("packet_loss_max", static_cast<double>(packetLoss));

    if (bitrateUnlocked)
        settings::SettingsStore::table(config, "debug").insert("unlock_bitrate_max", true);
}

const StreamSettings::Video& StreamSettings::video(StreamProfile profile) const
{
    switch (profile) {
        case StreamProfile::Remote: return remote();
        case StreamProfile::Vpn: return vpn();
        case StreamProfile::Local:
        case StreamProfile::Cloud:
        default: return local();
    }
}

int StreamSettings::defaultBitrate(VideoResolution resolution) const
{
    switch (resolution) {
        case VideoResolution::P2160: return 50000;
        case VideoResolution::P1440: return 30000;
        case VideoResolution::P1080: return fsr4k ? 25000 : 15000;
        case VideoResolution::P720: return 10000;
        case VideoResolution::P540: return 5000;
        case VideoResolution::P360: return 2000;
        default: return 10000;
    }
}

int StreamSettings::maxBitrate(VideoResolution resolution) const
{
    if (bitrateUnlocked) {
        switch (resolution) {
            case VideoResolution::P2160: return 120000;
            case VideoResolution::P1440: return 80000;
            case VideoResolution::P1080: return 50000;
            case VideoResolution::P720: return 40000;
            case VideoResolution::P540: return 10000;
            case VideoResolution::P360: return 5000;
            default: return 40000;
        }
    }
    switch (resolution) {
        case VideoResolution::P2160: return 60000;
        case VideoResolution::P1440: return 40000;
        case VideoResolution::P1080: return 40000;
        case VideoResolution::P720: return 20000;
        case VideoResolution::P540: return 10000;
        case VideoResolution::P360: return 5000;
        default: return 20000;
    }
}

int StreamSettings::minBitrate(VideoResolution resolution) const
{
    if (bitrateUnlocked) {
        switch (resolution) {
            case VideoResolution::P2160: return 10000;
            case VideoResolution::P1440: return 5000;
            case VideoResolution::P1080: return 5000;
            case VideoResolution::P720: return 5000;
            case VideoResolution::P540: return 1000;
            case VideoResolution::P360: return 1000;
            default: return 1000;
        }
    }
    return 1000;
}

bool StreamSettings::fsrEnabled(StreamProfile profile) const
{
    return video(profile).fsr;
}

int StreamSettings::fsrTarget(StreamProfile profile) const
{
    switch (profile) {
        case StreamProfile::Local: return local().fsrTarget;
        case StreamProfile::Remote: return remote().fsrTarget;
        case StreamProfile::Vpn: return vpn().fsrTarget;
        case StreamProfile::Cloud:
        default: return 0;
    }
}

void StreamSettings::setFsrTarget(StreamProfile profile, int height)
{
    switch (profile) {
        case StreamProfile::Local: local().fsrTarget = height; break;
        case StreamProfile::Remote: remote().fsrTarget = height; break;
        case StreamProfile::Vpn: vpn().fsrTarget = height; break;
        case StreamProfile::Cloud:
        default: break;
    }
}

int StreamSettings::rcasStrength() const
{
    return std::clamp(static_cast<int>(std::lround((1.0f - sharpness / 2.0f) * 100.0f)), 0, 100);
}

void StreamSettings::setRcasStrength(int percent)
{
    sharpness = (1.0f - static_cast<float>(std::clamp(percent, 0, 100)) / 100.0f) * 2.0f;
}

int StreamSettings::upscaleTargetHeight(StreamProfile profile, VideoResolution resolution) const
{
    if (const int target = fsrTarget(profile); target > 0 && (target <= 1080 || fsr4k))
        return target;
    return fsrTargetForResolution(resolution);
}

int StreamSettings::fsrTargetForResolution(VideoResolution resolution)
{
    switch (resolution) {
        case VideoResolution::P540:
            return 720;
        case VideoResolution::P720:
            return 1080;
        default:
            return 0;
    }
}

void StreamSettings::setDitheringStrength(float value)
{
    ditherStrength = std::max(1.0f, std::min(10.0f, value));
}

auto StreamSettings::levelName(DiagnosticsAppearance::Level level) -> const char*
{
    switch (level)
    {
        case DiagnosticsAppearance::Level::Off: return "off";
        case DiagnosticsAppearance::Level::Compact: return "compact";
        case DiagnosticsAppearance::Level::Standard: return "standard";
        case DiagnosticsAppearance::Level::Full: return "full";
    }
    return "off";
}

auto StreamSettings::glassName(DiagnosticsAppearance::Glass glass) -> const char*
{
    switch (glass)
    {
        case DiagnosticsAppearance::Glass::Regular: return "regular";
        case DiagnosticsAppearance::Glass::Clear: return "clear";
        case DiagnosticsAppearance::Glass::ClearDimmed: return "clearDimmed";
        case DiagnosticsAppearance::Glass::HealthTint: return "healthTint";
        case DiagnosticsAppearance::Glass::Solid: return "solid";
    }
    return "regular";
}

auto StreamSettings::layoutName(DiagnosticsAppearance::Layout layout) -> const char*
{
    return layout == DiagnosticsAppearance::Layout::Floating ? "floating" : "sheet";
}

template <typename Value, std::size_t Count>
auto StreamSettings::parsed(const std::array<Value, Count>& options, auto (*name)(Value)->const char*,
    std::string_view text, Value fallback) -> Value
{
    const auto found = std::ranges::find_if(options, [name, text](Value option) { return text == name(option); });
    return found == options.end() ? fallback : *found;
}

void StreamSettings::setStatsOverlayPosition(float x, float y)
{
    overlayX = std::max(0.0f, std::min(1.0f, x));
    overlayY = std::max(0.0f, std::min(1.0f, y));
}

}
