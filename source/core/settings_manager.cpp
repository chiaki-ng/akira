#include "core/settings_manager.hpp"
#include "core/swipe_direction.hpp"
#include "core/host.hpp"
#include "core/migrations/registry.hpp"
#include "core/core_log.hpp"
#include "input/remap_target.hpp"
#include "util/base64.hpp"
#include "util/http.hpp"

#include <borealis.hpp>
#include <format>
#include <ranges>
#include <utility>
#include <toml++/toml.hpp>
#include <switch.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sys/stat.h>

namespace {

using akira::input::RemapTarget;
using akira::input::remapKey;

ChiakiLogVerbosity chiakiLogVerbosityFromString(const std::string& value)
{
    if (value == "info")
        return ChiakiLogVerbosity::Info;
    if (value == "debug")
        return ChiakiLogVerbosity::Debug;
    if (value == "trace")
        return ChiakiLogVerbosity::Trace;
    return ChiakiLogVerbosity::Normal;
}

const char* chiakiLogVerbosityToString(ChiakiLogVerbosity verbosity)
{
    switch (verbosity) {
        case ChiakiLogVerbosity::Info:  return "info";
        case ChiakiLogVerbosity::Debug: return "debug";
        case ChiakiLogVerbosity::Trace: return "trace";
        case ChiakiLogVerbosity::Normal:
        default:                        return "normal";
    }
}

brls::LogLevel borealisLogLevel(ChiakiLogVerbosity verbosity)
{
    switch (verbosity) {
        case ChiakiLogVerbosity::Debug: return brls::LogLevel::LOG_DEBUG;
        case ChiakiLogVerbosity::Trace: return brls::LogLevel::LOG_VERBOSE;
        case ChiakiLogVerbosity::Normal:
        case ChiakiLogVerbosity::Info:
        default:                        return brls::LogLevel::LOG_INFO;
    }
}

akira::input::RumbleSource sourceFromLegacyPreset(HapticPreset preset)
{
    switch (preset) {
        case HapticPreset::Disabled:      return akira::input::RumbleSource::Off;
        case HapticPreset::ConsoleRumble: return akira::input::RumbleSource::Game;
        default:                          return akira::input::RumbleSource::Derived;
    }
}

int fsrTargetHeightForResolution(VideoResolution resolution) {
    switch (resolution) {
        case VideoResolution::P540:
            return 720;
        case VideoResolution::P720:
            return 1080;
        default:
            return 0;
    }
}

std::string hidButtonToConfigString(uint64_t button) {
    switch (button) {
        case HidNpadButton_A: return "A";
        case HidNpadButton_B: return "B";
        case HidNpadButton_X: return "X";
        case HidNpadButton_Y: return "Y";
        case HidNpadButton_L: return "L";
        case HidNpadButton_R: return "R";
        case HidNpadButton_ZL: return "ZL";
        case HidNpadButton_ZR: return "ZR";
        case HidNpadButton_Plus: return "Plus";
        case HidNpadButton_Minus: return "Minus";
        case HidNpadButton_StickL: return "L Stick";
        case HidNpadButton_StickR: return "R Stick";
        case HidNpadButton_LeftSL: return "SL(L)";
        case HidNpadButton_LeftSR: return "SR(L)";
        case HidNpadButton_RightSL: return "SL(R)";
        case HidNpadButton_RightSR: return "SR(R)";
        case HidNpadButton_Up: return "D-Up";
        case HidNpadButton_Down: return "D-Down";
        case HidNpadButton_Left: return "D-Left";
        case HidNpadButton_Right: return "D-Right";
        case HidNpadButton_StickLUp: return "LS-Up";
        case HidNpadButton_StickLDown: return "LS-Down";
        case HidNpadButton_StickLLeft: return "LS-Left";
        case HidNpadButton_StickLRight: return "LS-Right";
        case HidNpadButton_StickRUp: return "RS-Up";
        case HidNpadButton_StickRDown: return "RS-Down";
        case HidNpadButton_StickRLeft: return "RS-Left";
        case HidNpadButton_StickRRight: return "RS-Right";
        default: return "";
    }
}

uint64_t configStringToHidButton(const std::string& name) {
    if (name == "A") return HidNpadButton_A;
    if (name == "B") return HidNpadButton_B;
    if (name == "X") return HidNpadButton_X;
    if (name == "Y") return HidNpadButton_Y;
    if (name == "L") return HidNpadButton_L;
    if (name == "R") return HidNpadButton_R;
    if (name == "ZL") return HidNpadButton_ZL;
    if (name == "ZR") return HidNpadButton_ZR;
    if (name == "Plus") return HidNpadButton_Plus;
    if (name == "Minus") return HidNpadButton_Minus;
    if (name == "L Stick") return HidNpadButton_StickL;
    if (name == "R Stick") return HidNpadButton_StickR;
    if (name == "SL(L)") return HidNpadButton_LeftSL;
    if (name == "SR(L)") return HidNpadButton_LeftSR;
    if (name == "SL(R)") return HidNpadButton_RightSL;
    if (name == "SR(R)") return HidNpadButton_RightSR;
    if (name == "D-Up") return HidNpadButton_Up;
    if (name == "D-Down") return HidNpadButton_Down;
    if (name == "D-Left") return HidNpadButton_Left;
    if (name == "D-Right") return HidNpadButton_Right;
    if (name == "LS-Up") return HidNpadButton_StickLUp;
    if (name == "LS-Down") return HidNpadButton_StickLDown;
    if (name == "LS-Left") return HidNpadButton_StickLLeft;
    if (name == "LS-Right") return HidNpadButton_StickLRight;
    if (name == "RS-Up") return HidNpadButton_StickRUp;
    if (name == "RS-Down") return HidNpadButton_StickRDown;
    if (name == "RS-Left") return HidNpadButton_StickRLeft;
    if (name == "RS-Right") return HidNpadButton_StickRRight;
    return 0;
}

std::string chiakiButtonToConfigKey(uint32_t button) {
    switch (button) {
        case remapKey(RemapTarget::Cross): return "cross";
        case remapKey(RemapTarget::Moon): return "circle";
        case remapKey(RemapTarget::Box): return "square";
        case remapKey(RemapTarget::Pyramid): return "triangle";
        case remapKey(RemapTarget::L1): return "l1";
        case remapKey(RemapTarget::R1): return "r1";
        case remapKey(RemapTarget::L2): return "l2";
        case remapKey(RemapTarget::R2): return "r2";
        case remapKey(RemapTarget::L3): return "l3";
        case remapKey(RemapTarget::R3): return "r3";
        case remapKey(RemapTarget::Options): return "options";
        case remapKey(RemapTarget::Share): return "share";
        case remapKey(RemapTarget::Touchpad): return "touchpad";
        case remapKey(RemapTarget::Ps): return "ps";
        case SWIPE_TOUCHPAD_UP: return "swipe_up";
        case SWIPE_TOUCHPAD_DOWN: return "swipe_down";
        case SWIPE_TOUCHPAD_LEFT: return "swipe_left";
        case SWIPE_TOUCHPAD_RIGHT: return "swipe_right";
        default: return "";
    }
}

uint32_t configKeyToChiakiButton(const std::string& key) {
    if (key == "cross") return remapKey(RemapTarget::Cross);
    if (key == "circle") return remapKey(RemapTarget::Moon);
    if (key == "square") return remapKey(RemapTarget::Box);
    if (key == "triangle") return remapKey(RemapTarget::Pyramid);
    if (key == "l1") return remapKey(RemapTarget::L1);
    if (key == "r1") return remapKey(RemapTarget::R1);
    if (key == "l2") return remapKey(RemapTarget::L2);
    if (key == "r2") return remapKey(RemapTarget::R2);
    if (key == "l3") return remapKey(RemapTarget::L3);
    if (key == "r3") return remapKey(RemapTarget::R3);
    if (key == "options") return remapKey(RemapTarget::Options);
    if (key == "share") return remapKey(RemapTarget::Share);
    if (key == "touchpad") return remapKey(RemapTarget::Touchpad);
    if (key == "ps") return remapKey(RemapTarget::Ps);
    if (key == "swipe_up") return SWIPE_TOUCHPAD_UP;
    if (key == "swipe_down") return SWIPE_TOUCHPAD_DOWN;
    if (key == "swipe_left") return SWIPE_TOUCHPAD_LEFT;
    if (key == "swipe_right") return SWIPE_TOUCHPAD_RIGHT;
    return 0;
}

std::vector<cloud::Game> readShortcuts(const toml::array* arr) {
    std::vector<cloud::Game> out;
    if (!arr)
        return out;

    for (const auto& node : *arr) {
        const auto* t = node.as_table();
        if (!t) continue;

        cloud::Game g;
        g.productId = (*t)["product_id"].value<std::string>().value_or("");
        g.name = (*t)["name"].value<std::string>().value_or("");
        if (g.productId.empty() || g.name.empty()) continue;

        g.imageUrl = (*t)["image_url"].value<std::string>().value_or("");
        g.landscapeImageUrl = (*t)["landscape_image_url"].value<std::string>().value_or("");
        g.conceptId = (*t)["concept_id"].value<std::string>().value_or("");
        g.category = (*t)["category"].value<std::string>().value_or("");
        g.serviceType = (*t)["service_type"].value<std::string>().value_or("");
        g.platform = (*t)["platform"].value<std::string>().value_or("");
        g.isOwned = (*t)["is_owned"].value<bool>().value_or(false);
        g.streamServiceType = (*t)["stream_service_type"].value<std::string>().value_or("");
        g.streamIdentifier = (*t)["stream_identifier"].value<std::string>().value_or("");
        g.entitlementId = (*t)["entitlement_id"].value<std::string>().value_or("");
        g.storeProductId = (*t)["store_product_id"].value<std::string>().value_or("");
        g.conceptUrl = (*t)["concept_url"].value<std::string>().value_or("");
        g.plusCatalog = (*t)["plus_catalog"].value<bool>().value_or(false);
        out.push_back(std::move(g));
    }
    return out;
}

toml::array shortcutsToToml(const std::vector<cloud::Game>& shortcuts) {
    toml::array arr;
    for (const cloud::Game& g : shortcuts) {
        toml::table t;
        t.insert("product_id", g.productId);
        t.insert("name", g.name);
        if (!g.imageUrl.empty()) t.insert("image_url", g.imageUrl);
        if (!g.landscapeImageUrl.empty()) t.insert("landscape_image_url", g.landscapeImageUrl);
        if (!g.conceptId.empty()) t.insert("concept_id", g.conceptId);
        if (!g.category.empty()) t.insert("category", g.category);
        if (!g.serviceType.empty()) t.insert("service_type", g.serviceType);
        if (!g.platform.empty()) t.insert("platform", g.platform);
        if (g.isOwned) t.insert("is_owned", true);
        if (!g.streamServiceType.empty()) t.insert("stream_service_type", g.streamServiceType);
        if (!g.streamIdentifier.empty()) t.insert("stream_identifier", g.streamIdentifier);
        if (!g.entitlementId.empty()) t.insert("entitlement_id", g.entitlementId);
        if (!g.storeProductId.empty()) t.insert("store_product_id", g.storeProductId);
        if (!g.conceptUrl.empty()) t.insert("concept_url", g.conceptUrl);
        if (g.plusCatalog) t.insert("plus_catalog", true);
        arr.push_back(std::move(t));
    }
    return arr;
}

std::vector<cloud::Datacenter> readDatacenters(const toml::array* arr) {
    std::vector<cloud::Datacenter> out;
    if (!arr)
        return out;

    for (const auto& node : *arr) {
        const auto* t = node.as_table();
        if (!t) continue;

        cloud::Datacenter dc;
        dc.name = (*t)["name"].value<std::string>().value_or("");
        if (dc.name.empty()) continue;

        dc.rttMs = static_cast<int>((*t)["rtt"].value<int64_t>().value_or(0));
        dc.mtuIn = static_cast<int>((*t)["mtu_in"].value<int64_t>().value_or(0));
        dc.mtuOut = static_cast<int>((*t)["mtu_out"].value<int64_t>().value_or(0));
        dc.port = static_cast<int>((*t)["port"].value<int64_t>().value_or(0));
        dc.publicIp = (*t)["public_ip"].value<std::string>().value_or("");
        dc.maxBandwidth = static_cast<int>((*t)["max_bandwidth"].value<int64_t>().value_or(0));
        dc.measured = (*t)["measured"].value<bool>().value_or(false);
        dc.measuredAtMs = (*t)["measured_at"].value<int64_t>().value_or(0);

        if (const auto* rtts = t->get_as<toml::array>("rtts")) {
            for (const auto& rtt : *rtts) {
                if (auto val = rtt.value<int64_t>())
                    dc.rttSamples.push_back(static_cast<int>(*val));
            }
        }

        out.push_back(std::move(dc));
    }
    return out;
}

toml::array datacentersToToml(const std::vector<cloud::Datacenter>& datacenters) {
    toml::array arr;
    for (const cloud::Datacenter& dc : datacenters) {
        toml::table t;
        t.insert("name", dc.name);
        t.insert("rtt", dc.rttMs);

        toml::array rtts;
        for (int sample : dc.rttSamples)
            rtts.push_back(sample);
        t.insert("rtts", rtts);

        t.insert("mtu_in", dc.mtuIn);
        t.insert("mtu_out", dc.mtuOut);
        t.insert("port", dc.port);
        t.insert("public_ip", dc.publicIp);
        t.insert("max_bandwidth", dc.maxBandwidth);
        t.insert("measured", dc.measured);
        if (dc.measuredAtMs > 0) t.insert("measured_at", dc.measuredAtMs);
        arr.push_back(std::move(t));
    }
    return arr;
}

} // anonymous namespace



SettingsManager::SettingsManager() {
    buttonMapping = getDefaultButtonMapping();
}

void SettingsManager::setCoreLog(akira::CoreLog* log) {
    coreLog = log;
    updateChiakiLogMask();
}

void SettingsManager::ensureCloudDuid(libchiaking::platform::SecureRandomProvider& random) {
    constexpr std::string_view prefix = "0000000700410080";
    const bool validCloudDuid = cloudDuid.size() == 48 &&
        cloudDuid.starts_with(prefix) &&
        std::ranges::all_of(cloudDuid, [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    if (validCloudDuid)
        return;

    std::array<std::byte, 16> randomBytes{};
    if (!random.fill_bytes(randomBytes)) {
        brls::Logger::error("Failed to generate cloud DUID");
        cloudDuid.clear();
        return;
    }
    std::string generated(prefix);
    for (const std::byte byte : randomBytes)
        std::format_to(std::back_inserter(generated), "{:02x}", std::to_integer<unsigned>(byte));
    cloudDuid = std::move(generated);
    if (writeFile() != 0)
        brls::Logger::error("Failed to persist cloud DUID");
}

SettingsManager* SettingsManager::getInstance() {
    static SettingsManager* instance = nullptr;
    if (!instance) {
        instance = new SettingsManager();
        instance->ensureConfigDir();
        instance->parseFile();
        instance->owned = instance->ownedSettings();
    }
    return instance;
}

void SettingsManager::ensureConfigDir() {
    mkdir(CONFIG_DIR, 0755);
}

bool SettingsManager::fileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

std::map<std::string, std::unique_ptr<Host>>* SettingsManager::getHostsMap() {
    return &hosts;
}

Host* SettingsManager::getOrCreateHost(const std::string& hostName) {
    bool created = false;

    if (hosts.find(hostName) == hosts.end()) {
        hosts[hostName] = std::make_unique<Host>(hostName);
        created = true;
    }

    Host* host = hosts.at(hostName).get();

    if (created && host->consoleId == 0) {
        host->consoleId = nextConsoleId++;
    }

    return host;
}

void SettingsManager::removeHost(const std::string& hostName) {
    hosts.erase(hostName);
}

void SettingsManager::removeActiveProfileRegistration(const std::string& hostName) {
    auto it = hosts.find(hostName);
    if (it == hosts.end() || !it->second)
        return;

    int64_t pid = getActiveProfileId();
    it->second->removeRegistrationsForProfile(pid);

    if (!it->second->hasRpKey())
        hosts.erase(it);
}

void SettingsManager::renameHost(const std::string& oldName, const std::string& newName) {
    auto it = hosts.find(oldName);
    if (it == hosts.end()) return;

    auto host = std::move(it->second);
    hosts.erase(it);
    host->hostName = newName;
    hosts[newName] = std::move(host);
}

Host* SettingsManager::findHostByDuid(const std::string& duid) {
    if (duid.empty()) {
        return nullptr;
    }
    for (auto& [name, host] : hosts) {
        if (host && host->getRemoteDuid() == duid) {
            return host.get();
        }
    }
    return nullptr;
}

void SettingsManager::parseFile() {
    if (fileExists(TOML_CONFIG_FILE)) {
        parseTomlFile();
    } else if (fileExists(LEGACY_CONFIG_FILE)) {
        brls::Logger::info("Migrating from legacy config format");
        parseLegacyFile();
        writeFile();
    } else {
        brls::Logger::info("No config file found, using defaults");
    }
}

void SettingsManager::parseTomlFile() {
    brls::Logger::info("Parsing TOML config file: {}", TOML_CONFIG_FILE);

    try {
        auto config = toml::parse_file(TOML_CONFIG_FILE);

        // Best-effort: on failure (e.g. newer config version) load fields as-is
        // and skip the rewrite rather than aborting startup.
        bool configMigrated = false;
        const int64_t configVersion = config["version"].value_or(int64_t{0});
        try {
            configMigrated =
                chiaki_migrations::buildSettingsMigrator().migrate(config).changed();
        } catch (const tomlmigrate::MigrationError& err) {
            brls::Logger::error(
                "Config migration skipped, loading fields as-is: {}", err.what());
        }

        if (auto val = config["video"]["local"]["resolution"].value<std::string>())
            localVideoResolution = stringToResolution(*val);
        if (auto val = config["video"]["remote"]["resolution"].value<std::string>())
            remoteVideoResolution = stringToResolution(*val);
        if (auto val = config["video"]["vpn"]["resolution"].value<std::string>())
            vpnVideoResolution = stringToResolution(*val);
        cloudVideoResolutionPscloud =
            config["video"]["cloud"]["pscloud"]["resolution"].value<int64_t>().value_or(1080);
        cloudVideoResolutionPsnow =
            config["video"]["cloud"]["psnow"]["resolution"].value<int64_t>().value_or(1080);

        if (auto val = config["video"]["local"]["fps"].value<int64_t>())
            localVideoFPS = stringToFps(std::to_string(*val));
        if (auto val = config["video"]["remote"]["fps"].value<int64_t>())
            remoteVideoFPS = stringToFps(std::to_string(*val));
        if (auto val = config["video"]["vpn"]["fps"].value<int64_t>())
            vpnVideoFPS = (*val == 30) ? VideoFps::Fps30 : VideoFps::Fps60;

        if (auto val = config["video"]["local"]["bitrate"].value<int64_t>())
            localVideoBitrate = static_cast<int>(*val);
        else
            localVideoBitrate = getDefaultBitrateForResolution(localVideoResolution);

        if (auto val = config["video"]["remote"]["bitrate"].value<int64_t>())
            remoteVideoBitrate = static_cast<int>(*val);
        else
            remoteVideoBitrate = getDefaultBitrateForResolution(remoteVideoResolution);

        if (auto val = config["video"]["vpn"]["bitrate"].value<int64_t>())
            vpnVideoBitrate = static_cast<int>(*val);
        cloudVideoBitratePscloud =
            config["video"]["cloud"]["pscloud"]["bitrate"].value<int64_t>().value_or(10000);
        cloudVideoBitratePsnow =
            config["video"]["cloud"]["psnow"]["bitrate"].value<int64_t>().value_or(10000);

        if (auto val = config["video"]["local"]["fsr_enabled"].value<bool>())
            localFsrEnabled = *val;
        if (auto val = config["video"]["remote"]["fsr_enabled"].value<bool>())
            remoteFsrEnabled = *val;
        if (auto val = config["video"]["vpn"]["fsr_enabled"].value<bool>())
            vpnFsrEnabled = *val;
        if (auto val = config["video"]["cloud"]["pscloud"]["fsr_enabled"].value<bool>())
            cloudFsrEnabledPscloud = *val;
        if (auto val = config["video"]["cloud"]["psnow"]["fsr_enabled"].value<bool>())
            cloudFsrEnabledPsnow = *val;

        if (auto val = config["picture"]["dithering_enabled"].value<bool>())
            enableDithering = *val;
        if (auto val = config["picture"]["dithering_strength"].value<double>())
            ditheringStrength = std::max(1.0f, std::min(10.0f, static_cast<float>(*val)));
        if (auto val = config["picture"]["rcas_enabled"].value<bool>())
            rcasEnabled = *val;
        if (auto val = config["picture"]["rcas_sharpness"].value<double>())
            rcasSharpness = static_cast<float>(*val);

        if (auto val = config["overlay"]["stats_x"].value<double>())
            statsOverlayX = static_cast<float>(*val);
        if (auto val = config["overlay"]["stats_y"].value<double>())
            statsOverlayY = static_cast<float>(*val);

        if (auto val = config["input"]["haptic"].value<int64_t>())
            globalHaptic = static_cast<HapticPreset>(*val);
        if (auto val = config["input"]["direct_rumble_in_stream"].value<bool>())
            directRumbleInStream = *val;
        if (auto val = config["input"]["direct_haptics_in_stream"].value<bool>())
            directHapticsInStream = *val;
        if (auto val = config["input"]["gyro_source"].value<int64_t>())
            globalGyroSource = static_cast<GyroSource>(*val);

        if (auto rumbleTable = config["input"]["rumble"].as_table()) {
            for (const auto& [key, node] : *rumbleTable) {
                const auto* entry = node.as_table();
                if (entry == nullptr)
                    continue;

                std::string name(key.str());
                akira::input::RumbleProfile profile =
                    (name == akira::input::kRumbleKeySwitch)
                        ? akira::input::SwitchRumbleProfile()
                        : akira::input::DefaultRumbleProfile();

                if (auto v = (*entry)["strength"].value<double>())
                    profile.strength = std::max(0.0f, std::min(1.0f, static_cast<float>(*v)));
                if (auto v = (*entry)["ceiling"].value<double>())
                    profile.ceiling = std::max(0.0f, std::min(1.0f, static_cast<float>(*v)));
                if (auto v = (*entry)["per_motor"].value<bool>())
                    profile.per_motor = *v;
                if (auto v = (*entry)["freq_low"].value<double>())
                    profile.freq_low = std::max(40.0f, std::min(320.0f, static_cast<float>(*v)));
                if (auto v = (*entry)["freq_high"].value<double>())
                    profile.freq_high = std::max(40.0f, std::min(320.0f, static_cast<float>(*v)));
                if (auto v = (*entry)["envelope_attack"].value<double>())
                    profile.envelope_attack = std::max(0.20f, std::min(1.00f, static_cast<float>(*v)));
                if (auto v = (*entry)["envelope_decay"].value<double>())
                    profile.envelope_decay = std::max(0.50f, std::min(0.95f, static_cast<float>(*v)));
                if (auto v = (*entry)["haptic_intensity"].value<int64_t>()) {
                    const int64_t clamped = std::max<int64_t>(0, std::min<int64_t>(5, *v));
                    profile.haptic_intensity = static_cast<akira::input::HapticIntensity>(clamped);
                }
                if (auto v = (*entry)["direct_output"].value<bool>())
                    profile.output_mode = *v ? akira::input::PadOutputMode::Native
                                             : akira::input::PadOutputMode::Basic;
                if (auto v = (*entry)["output_mode"].value<int64_t>())
                    profile.output_mode = (*v == 1) ? akira::input::PadOutputMode::Basic
                                                    : akira::input::PadOutputMode::Native;

                if (auto v = (*entry)["lightbar_enabled"].value<bool>())
                    profile.lightbar_enabled = *v;
                if (auto v = (*entry)["lightbar_r"].value<int64_t>())
                    profile.lightbar_r = (std::uint8_t)std::clamp<int64_t>(*v, 0, 255);
                if (auto v = (*entry)["lightbar_g"].value<int64_t>())
                    profile.lightbar_g = (std::uint8_t)std::clamp<int64_t>(*v, 0, 255);
                if (auto v = (*entry)["lightbar_b"].value<int64_t>())
                    profile.lightbar_b = (std::uint8_t)std::clamp<int64_t>(*v, 0, 255);

                auto source = (*entry)["rumble_source"].value<int64_t>();
                if (!source)
                    source = (*entry)["console_feedback"].value<int64_t>();
                if (source) {
                    const int64_t clamped = std::max<int64_t>(0, std::min<int64_t>(2, *source));
                    profile.rumble_source = static_cast<akira::input::RumbleSource>(clamped);
                } else {
                    profile.rumble_source = sourceFromLegacyPreset(globalHaptic);
                    if (globalHaptic == HapticPreset::Weak)
                        profile.haptic_intensity = akira::input::HapticIntensity::Weak;
                }

                rumbleProfiles[name] = profile;
            }

            if (auto val = (*rumbleTable)["freq_low"].value<double>())
                rumbleFreqLow = std::max(40.0f, std::min(320.0f, static_cast<float>(*val)));
            if (auto val = (*rumbleTable)["freq_high"].value<double>())
                rumbleFreqHigh = std::max(40.0f, std::min(320.0f, static_cast<float>(*val)));
            if (auto val = (*rumbleTable)["envelope_decay"].value<double>())
                rumbleEnvelopeDecay = std::max(0.50f, std::min(0.95f, static_cast<float>(*val)));
            if (auto val = (*rumbleTable)["envelope_attack"].value<double>())
                rumbleEnvelopeAttack = std::max(0.20f, std::min(1.00f, static_cast<float>(*val)));
        }

        if (rumbleProfiles.find(akira::input::kRumbleKeySwitch) == rumbleProfiles.end()) {
            akira::input::RumbleProfile migrated = akira::input::SwitchRumbleProfile();
            migrated.freq_low        = rumbleFreqLow;
            migrated.freq_high       = rumbleFreqHigh;
            migrated.envelope_attack = rumbleEnvelopeAttack;
            migrated.envelope_decay  = rumbleEnvelopeDecay;

            rumbleProfiles[akira::input::kRumbleKeySwitch] = migrated;
            brls::Logger::info("rumble profiles: migrated existing settings into the switch profile");
        }

        {
            const auto old_switch = rumbleProfiles.find(akira::input::kRumbleKeySwitch);
            const akira::input::RumbleProfile inherited =
                old_switch != rumbleProfiles.end() ? old_switch->second
                                                   : akira::input::SwitchRumbleProfile();

            for (const char* key : { akira::input::kRumbleKeyJoyCon,
                                     akira::input::kRumbleKeySwitchPro }) {
                if (rumbleProfiles.find(key) == rumbleProfiles.end())
                    rumbleProfiles[key] = inherited;
            }
        }

        if (rumbleProfiles.find(akira::input::kRumbleKeyDualSense) == rumbleProfiles.end()) {
            akira::input::RumbleProfile ds = akira::input::DefaultRumbleProfile();

            ds.output_mode    = akira::input::PadOutputMode::Native;
            ds.rumble_source  = sourceFromLegacyPreset(globalHaptic);
            rumbleProfiles[akira::input::kRumbleKeyDualSense] = ds;
        }
        if (rumbleProfiles.find(akira::input::kRumbleKeyGeneric) == rumbleProfiles.end())
            rumbleProfiles[akira::input::kRumbleKeyGeneric] = akira::input::DefaultRumbleProfile();
        if (rumbleProfiles.find(akira::input::kRumbleKeyDefault) == rumbleProfiles.end())
            rumbleProfiles[akira::input::kRumbleKeyDefault] = akira::input::DefaultRumbleProfile();

        cloudDuid = config["cloud"]["duid"].value<std::string>().value_or("");
        {
            std::lock_guard<std::mutex> datacenterLock(datacenterMutex);
            cloudDatacentersPscloud = readDatacenters(config["cloud"]["datacenters"]["pscloud"].as_array());
            cloudDatacentersPsnow = readDatacenters(config["cloud"]["datacenters"]["psnow"].as_array());
        }
        cloudSortState = static_cast<int>(config["cloud"]["sort_state"].value<int64_t>().value_or(0));
        cloudFilter = config["cloud"]["filter"].value<std::string>().value_or("");

        if (auto val = config["network"]["holepunch_retry"].value<bool>())
            holepunchRetry = *val;
        if (auto val = config["network"]["port_guessing"].value<bool>())
            portGuessing = *val;
        if (auto val = config["network"]["port_guessing_count"].value<int64_t>())
            portGuessingCount = static_cast<int>(*val);
        if (auto val = config["network"]["port_guessing_socks"].value<int64_t>())
            portGuessingSocks = static_cast<int>(*val);
        if (auto val = config["network"]["discovery_subnets"].value<std::string>())
            discoverySubnets = *val;
        if (auto val = config["network"]["companion_port"].value<int64_t>())
            companionPort = static_cast<int>(*val);

        if (auto val = config["stream"]["auto_reconnect"].value<bool>())
            autoReconnect = *val;
        if (auto val = config["stream"]["sleep_on_exit"].value<bool>())
            sleepOnExit = *val;
        if (auto val = config["stream"]["request_idr_on_fec_failure"].value<bool>())
            requestIdrOnFecFailure = *val;
        for (const auto target : {TakionTarget::PS4, TakionTarget::PS5, TakionTarget::PS5Cloud, TakionTarget::PSNow}) {
            if (auto val = config["stream"]["takion_versions"][takionTargetKey(target)].value<int64_t>())
                takionVersions[static_cast<size_t>(target)] = static_cast<int>(*val);
        }
        if (auto val = config["stream"]["packet_loss_max"].value<double>())
            packetLossMax = static_cast<float>(*val);

        if (auto val = config["ui"]["theme"].value<std::string>())
            uiTheme = *val;
        if (auto val = config["ui"]["hide_account_name"].value<bool>())
            hideAccountName = *val;
        if (auto val = config["ui"]["connection_show_stages"].value<bool>())
            connectionShowStages = *val;

        if (auto val = config["updates"]["channel"].value<std::string>())
            updateChannel = *val;
        if (auto val = config["updates"]["auto_check"].value<bool>())
            autoCheckUpdates = *val;
        if (auto val = config["updates"]["last_check"].value<int64_t>())
            lastUpdateCheck = *val;
        if (auto val = config["updates"]["install_path"].value<std::string>())
            updateInstallPath = *val;

        if (auto val = config["debug"]["locale"].value<std::string>())
            debugLocale = *val;
        if (auto val = config["debug"]["file_logging"].value<bool>())
            enableFileLogging = *val;
        if (auto val = config["debug"]["silence_all_logs"].value<bool>())
            silenceAllLogs = *val;
        if (auto val = config["debug"]["lwip_log"].value<bool>())
            debugLwipLog = *val;
        if (auto val = config["debug"]["wireguard_log"].value<bool>())
            debugWireguardLog = *val;
        if (auto val = config["debug"]["render_log"].value<bool>())
            debugRenderLog = *val;
        if (auto val = config["debug"]["chiaki_log_level"].value<std::string>())
            chiakiLogVerbosity = chiakiLogVerbosityFromString(*val);
        else if (auto val = config["debug"]["chiaki_log"].value<bool>())
            chiakiLogVerbosity = *val ? ChiakiLogVerbosity::Trace
                                      : ChiakiLogVerbosity::Normal;
        if (auto val = config["debug"]["discovery_log"].value<bool>())
            debugDiscoveryLog = *val;
        if (auto val = config["debug"]["ffmpeg_log"].value<bool>())
            debugFfmpegLog = *val;
        if (auto val = config["debug"]["ipc_stats"].value<bool>())
            ipcStatsEnabled = *val;
        if (auto val = config["debug"]["fake_hosts"].value<bool>())
            devFakeHosts = *val;
        if (auto val = config["debug"]["power_user_menu_unlocked"].value<bool>())
            powerUserMenuUnlocked = *val;
        if (auto val = config["debug"]["unlock_bitrate_max"].value<bool>())
            unlockBitrateMax = *val;
        if (auto val = config["debug"]["update_server"].value<std::string>())
            devUpdateServer = *val;

        if (auto mappingTable = config["input"]["button_mapping"].as_table()) {
            for (auto& [key, value] : *mappingTable) {
                std::string keyStr(key.str());
                uint32_t chiakiBtn = configKeyToChiakiButton(keyStr);
                if (chiakiBtn == 0) continue;

                if (auto* arr = value.as_array()) {
                    std::vector<uint64_t> combo;
                    for (auto& elem : *arr) {
                        if (auto btnName = elem.value<std::string>()) {
                            uint64_t hidBtn = configStringToHidButton(*btnName);
                            if (hidBtn != 0) {
                                combo.push_back(hidBtn);
                            }
                        }
                    }
                    buttonMapping[chiakiBtn] = combo;
                }
            }
            if (auto val = (*mappingTable)["touchpad_enabled"].value<bool>())
                touchpadEnabled = *val;
            if (auto val = (*mappingTable)["swipe_up_enabled"].value<bool>())
                swipeUpEnabled = *val;
            if (auto val = (*mappingTable)["swipe_down_enabled"].value<bool>())
                swipeDownEnabled = *val;
            if (auto val = (*mappingTable)["swipe_left_enabled"].value<bool>())
                swipeLeftEnabled = *val;
            if (auto val = (*mappingTable)["swipe_right_enabled"].value<bool>())
                swipeRightEnabled = *val;

            brls::Logger::info("Loaded button mapping from config");
        }

        if (auto* profilesArr = config["profiles"].as_array()) {
            for (auto& elem : *profilesArr) {
                auto* pt = elem.as_table();
                if (!pt) continue;

                Profile profile;
                profile.id = (*pt)["profile_id"].value<int64_t>().value_or(0);
                if (profile.id <= 0) continue;

                profile.onlineId = (*pt)["online_id"].value<std::string>().value_or("");
                profile.accountId = (*pt)["account_id"].value<std::string>().value_or("");
                profile.avatarUrl = (*pt)["avatar_url"].value<std::string>().value_or("");
                profile.trophiesEnabled = (*pt)["trophies_enabled"].value<bool>().value_or(true);
                profile.legacy = (*pt)["legacy"].value<bool>().value_or(false);
                profile.cloudShortcuts = readShortcuts((*pt)["cloud_shortcuts"].as_array());

                profiles.push_back(profile);
                if (profile.id >= nextProfileId)
                    nextProfileId = profile.id + 1;
            }
        }

        if (auto val = config["active_profile_id"].value<int64_t>())
            activeProfileId = *val;

        if (auto* consolesArr = config["consoles"].as_array()) {
            for (auto& elem : *consolesArr) {
                auto* ct = elem.as_table();
                if (!ct) continue;

                std::string nickname = (*ct)["nickname"].value<std::string>().value_or("");
                if (nickname.empty()) continue;

                Host* host = getOrCreateHost(nickname);
                host->inConfig = true;
                host->hostName = nickname;
                host->serverNickname = (*ct)["server_nickname"].value<std::string>().value_or("");
                host->consoleId = (*ct)["console_id"].value<int64_t>().value_or(host->consoleId);
                if (host->consoleId >= nextConsoleId)
                    nextConsoleId = host->consoleId + 1;

                if (auto val = (*ct)["host_type"].value<int64_t>())
                    host->hostType = static_cast<HostType>(*val);
                if (auto val = (*ct)["host_addr"].value<std::string>())
                    host->hostAddr = *val;
                if (auto val = (*ct)["target"].value<int64_t>())
                    host->firmware = firmwareFromTarget(*val);
                if (auto val = (*ct)["hardware_id"].value<std::string_view>())
                    host->hardwareId = libchiaking::consoles::HardwareId::parse(*val);
                if (auto val = (*ct)["console_pin"].value<std::string>())
                    host->consolePIN = *val;
                if (auto val = (*ct)["haptic"].value<int64_t>())
                    host->haptic = static_cast<int>(*val);
                if (auto val = (*ct)["remote_duid"].value<std::string>())
                    host->remoteDuid = *val;
            }
        }

        if (auto* regsArr = config["registrations"].as_array()) {
            for (auto& elem : *regsArr) {
                auto* rt = elem.as_table();
                if (!rt) continue;

                int64_t consoleId = (*rt)["console_id"].value<int64_t>().value_or(0);
                Host* host = nullptr;
                for (auto& [n, h] : hosts) {
                    if (h && h->consoleId == consoleId) { host = h.get(); break; }
                }
                if (!host) continue;

                auto reg = registrationOf(*rt);
                if (!reg)
                    continue;

                host->removeRegistrationsForProfile(reg->profileId);
                host->legacyRegistrations.push_back(std::move(*reg));
            }
        }

        signInStore.read(config);
        pairingStore.read(config);

        brls::Logger::info("Loaded {} profile(s), {} console(s) from TOML config",
            profiles.size(), hosts.size());
        logProfileCensus("load");

        refreshLegacyGate();

        if (configMigrated) {
            brls::Logger::info("Config migrated to current schema, rewriting");
            backupConfig(configVersion);
            writeFile();
        }
    } catch (const toml::parse_error& err) {
        brls::Logger::error("Failed to parse TOML config: {}", err.what());
    } catch (const std::exception& err) {
        brls::Logger::error("Failed to load TOML config: {}", err.what());
    }
}

void SettingsManager::parseLegacyFile() {
    brls::Logger::info("Parsing legacy config file: {}", LEGACY_CONFIG_FILE);

    std::ifstream configFile(LEGACY_CONFIG_FILE);
    if (!configFile.is_open()) {
        brls::Logger::error("Failed to open legacy config file");
        return;
    }

    enum class ConfigItem {
        Unknown, HostName, HostAddr, PsnOnlineId, PsnAccountId, PsnRefreshToken,
        PsnAccessToken, ConsolePIN, RpKey, RpKeyType, RpRegistKey, VideoResolution,
        VideoFps, Target, Haptic, RemoteDuid, CompanionPort,
        PsnTokenExpiresAt, GlobalDuid
    };

    const std::map<ConfigItem, std::regex> regexMap = {
        {ConfigItem::HostName, std::regex("^\\[\\s*(.+)\\s*\\]")},
        {ConfigItem::HostAddr, std::regex("^\\s*host_(?:ip|addr)\\s*=\\s*\"?((\\d+\\.\\d+\\.\\d+\\.\\d+)|([A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)+))\"?")},
        {ConfigItem::PsnOnlineId, std::regex("^\\s*psn_online_id\\s*=\\s*\"?([\\w_-]+)\"?")},
        {ConfigItem::PsnAccountId, std::regex("^\\s*psn_account_id\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::PsnRefreshToken, std::regex("^\\s*psn_refresh_token\\s*=\\s*\"?([\\w._-]+)\"?")},
        {ConfigItem::PsnAccessToken, std::regex("^\\s*psn_access_token\\s*=\\s*\"?([\\w._-]+)\"?")},
        {ConfigItem::ConsolePIN, std::regex("^\\s*console_pin\\s*=\\s*\"?(\\d{4})\"?")},
        {ConfigItem::RpKey, std::regex("^\\s*rp_key\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::RpKeyType, std::regex("^\\s*rp_key_type\\s*=\\s*\"?(\\d)\"?")},
        {ConfigItem::RpRegistKey, std::regex("^\\s*rp_regist_key\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::VideoResolution, std::regex("^\\s*video_resolution\\s*=\\s*\"?(1080p|720p|540p|360p)\"?")},
        {ConfigItem::VideoFps, std::regex("^\\s*video_fps\\s*=\\s*\"?(60|30)\"?")},
        {ConfigItem::Target, std::regex("^\\s*target\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::Haptic, std::regex("^\\s*haptic\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::RemoteDuid, std::regex("^\\s*remote_duid\\s*=\\s*\"?([0-9a-fA-F]+)\"?")},
        {ConfigItem::CompanionPort, std::regex("^\\s*companion_port\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::PsnTokenExpiresAt, std::regex("^\\s*psn_token_expires_at\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::GlobalDuid, std::regex("^\\s*global_duid\\s*=\\s*\"?([0-9a-fA-F]+)\"?")}
    };

    auto parseLine = [&regexMap](const std::string& line, std::string& value) -> ConfigItem {
        std::smatch match;
        for (const auto& [item, regex] : regexMap) {
            if (std::regex_search(line, match, regex)) {
                value = match[1];
                return item;
            }
        }
        return ConfigItem::Unknown;
    };

    std::string line;
    std::string value;
    Host* currentHost = nullptr;
    bool rpKeySet = false, rpRegistKeySet = false, rpKeyTypeSet = false;
    std::array<std::byte, libchiaking::consoles::PairingKeys::registration_key_size> legacyRegistKey{};
    std::array<std::byte, libchiaking::consoles::PairingKeys::remote_play_key_size> legacyKey{};
    uint32_t legacyKeyType = 0;

    while (std::getline(configFile, line)) {
        ConfigItem item = parseLine(line, value);

        switch (item) {
            case ConfigItem::Unknown:
                break;
            case ConfigItem::HostName: {
                std::string cleanName = value;
                HostType migratedType = HostType::Discovered;
                if (value.length() > 9 && value.substr(value.length() - 9) == " (Remote)") {
                    cleanName = value;
                    migratedType = HostType::Remote;
                } else if (value.rfind("[Manual] ", 0) == 0) {
                    cleanName = value.substr(9);
                    migratedType = HostType::Manual;
                } else if (value.rfind("[Auto] ", 0) == 0) {
                    cleanName = value.substr(7);
                    migratedType = HostType::Auto;
                }
                if (cleanName != value && hosts.find(cleanName) != hosts.end()) {
                    Host* existing = hosts[cleanName].get();
                    if (existing->hostType == HostType::Manual && migratedType != HostType::Manual) {
                        brls::Logger::info("Skipping {} - Manual host {} already exists", value, cleanName);
                        currentHost = nullptr;
                        break;
                    }
                }
                currentHost = getOrCreateHost(cleanName);
                currentHost->inConfig = true;
                currentHost->hostType = migratedType;
                currentHost->hostName = cleanName;
                rpKeySet = rpRegistKeySet = rpKeyTypeSet = false;
                break;
            }
            case ConfigItem::HostAddr:
                if (currentHost) currentHost->hostAddr = value;
                break;
            case ConfigItem::PsnOnlineId:
                setPsnOnlineId(currentHost, value);
                break;
            case ConfigItem::PsnAccountId:
                setPsnAccountId(currentHost, value);
                break;
            case ConfigItem::PsnRefreshToken:
            case ConfigItem::PsnAccessToken:
                break;
            case ConfigItem::ConsolePIN:
                if (currentHost) currentHost->consolePIN = value;
                break;
            case ConfigItem::RpKey:
                if (currentHost)
                    rpKeySet = akira::base64::decodeExactly(value, legacyKey);
                break;
            case ConfigItem::RpKeyType:
                if (currentHost) {
                    legacyKeyType = static_cast<uint32_t>(std::atoi(value.c_str()));
                    rpKeyTypeSet = true;
                }
                break;
            case ConfigItem::RpRegistKey:
                if (currentHost)
                    rpRegistKeySet = akira::base64::decodeExactly(value, legacyRegistKey);
                break;
            case ConfigItem::VideoResolution:
                localVideoResolution = stringToResolution(value);
                remoteVideoResolution = stringToResolution(value);
                break;
            case ConfigItem::VideoFps:
                localVideoFPS = stringToFps(value);
                remoteVideoFPS = stringToFps(value);
                break;
            case ConfigItem::Haptic:
                setHaptic(currentHost, value);
                break;
            case ConfigItem::RemoteDuid:
                if (currentHost) {
                    currentHost->remoteDuid = value;
                }
                break;
            case ConfigItem::Target:
                if (currentHost) currentHost->firmware = firmwareFromTarget(std::atoll(value.c_str()));
                break;
            case ConfigItem::CompanionPort:
                companionPort = std::atoi(value.c_str());
                if (companionPort <= 0 || companionPort > 65535) companionPort = 8080;
                break;
            case ConfigItem::PsnTokenExpiresAt:
            case ConfigItem::GlobalDuid:
                break;
        }

        if (rpKeySet && rpRegistKeySet && rpKeyTypeSet && currentHost) {
            if (auto keys = libchiaking::consoles::PairingKeys::create(legacyRegistKey, legacyKey, legacyKeyType)) {
                const int64_t profileId = ensureActiveProfile()->id;
                currentHost->removeRegistrationsForProfile(profileId);
                currentHost->legacyRegistrations.push_back(Registration{profileId, std::move(*keys)});
            }
            rpKeySet = rpRegistKeySet = rpKeyTypeSet = false;
        }
    }

    configFile.close();
    libchiaking::memory::secure_zero(std::span<std::byte>{legacyRegistKey});
    libchiaking::memory::secure_zero(std::span<std::byte>{legacyKey});
    brls::Logger::info("Loaded {} host(s) from legacy config", hosts.size());
}

std::mutex SettingsManager::writeMutex;
std::atomic<uint64_t> SettingsManager::writeSeq{0};

int SettingsManager::writeFile() {
    std::lock_guard<std::mutex> writeLock(writeMutex);

    brls::Logger::info("Writing config file: {}", TOML_CONFIG_FILE);

    ensureConfigDir();
    owned = ownedSettings();
    return persist(owned);
}

int SettingsManager::writeStores() {
    std::lock_guard<std::mutex> writeLock(writeMutex);

    brls::Logger::info("Writing the stores to {}", TOML_CONFIG_FILE);

    ensureConfigDir();
    return persist(owned);
}

toml::table SettingsManager::ownedSettings() {
    toml::table config;

    config.insert("version", chiaki_migrations::buildSettingsMigrator().latest_version());

    {
        auto videoProfile = [](const std::string& resolution, int fps, int bitrate, bool fsr) {
            toml::table t;
            t.insert("resolution", resolution);
            t.insert("fps", fps);
            t.insert("bitrate", bitrate);
            t.insert("fsr_enabled", fsr);
            return t;
        };

        auto cloudVideoProfile = [](int resolution, int bitrate, bool fsr) {
            toml::table t;
            t.insert("resolution", resolution);
            t.insert("bitrate", bitrate);
            t.insert("fsr_enabled", fsr);
            return t;
        };

        toml::table cloudVideo;
        cloudVideo.insert("pscloud", cloudVideoProfile(cloudVideoResolutionPscloud,
            cloudVideoBitratePscloud, cloudFsrEnabledPscloud));
        cloudVideo.insert("psnow", cloudVideoProfile(cloudVideoResolutionPsnow,
            cloudVideoBitratePsnow, cloudFsrEnabledPsnow));

        toml::table video;
        video.insert("local", videoProfile(resolutionToString(localVideoResolution),
            fpsToInt(localVideoFPS), localVideoBitrate, localFsrEnabled));
        video.insert("remote", videoProfile(resolutionToString(remoteVideoResolution),
            fpsToInt(remoteVideoFPS), remoteVideoBitrate, remoteFsrEnabled));
        video.insert("vpn", videoProfile(resolutionToString(vpnVideoResolution),
            fpsToInt(vpnVideoFPS), vpnVideoBitrate, vpnFsrEnabled));
        video.insert("cloud", std::move(cloudVideo));
        config.insert("video", std::move(video));
    }

    {
        toml::table picture;
        picture.insert("dithering_enabled", enableDithering);
        picture.insert("dithering_strength", static_cast<double>(ditheringStrength));
        picture.insert("rcas_enabled", rcasEnabled);
        picture.insert("rcas_sharpness", static_cast<double>(rcasSharpness));
        config.insert("picture", std::move(picture));
    }

    if (statsOverlayX >= 0.0f && statsOverlayY >= 0.0f)
    {
        toml::table overlay;
        overlay.insert("stats_x", static_cast<double>(statsOverlayX));
        overlay.insert("stats_y", static_cast<double>(statsOverlayY));
        config.insert("overlay", std::move(overlay));
    }

    {
        toml::table cloud;
        if (!cloudDuid.empty()) cloud.insert("duid", cloudDuid);
        if (cloudSortState != 0) cloud.insert("sort_state", cloudSortState);
        if (!cloudFilter.empty()) cloud.insert("filter", cloudFilter);

        if (!cloud.empty())
            config.insert("cloud", std::move(cloud));
    }

    {
        toml::table network;
        if (holepunchRetry) network.insert("holepunch_retry", true);
        network.insert("port_guessing", portGuessing);
        network.insert("port_guessing_count", portGuessingCount);
        network.insert("port_guessing_socks", portGuessingSocks);
        if (!discoverySubnets.empty()) network.insert("discovery_subnets", discoverySubnets);
        network.insert("companion_port", companionPort);
        config.insert("network", std::move(network));
    }

    {
        toml::table stream;
        if (!autoReconnect) stream.insert("auto_reconnect", false);
        if (sleepOnExit) stream.insert("sleep_on_exit", true);
        stream.insert("request_idr_on_fec_failure", requestIdrOnFecFailure);
        toml::table versions;
        for (const auto target : {TakionTarget::PS4, TakionTarget::PS5, TakionTarget::PS5Cloud, TakionTarget::PSNow}) {
            if (const int version = getTakionVersion(target))
                versions.insert(takionTargetKey(target), static_cast<int64_t>(version));
        }
        if (!versions.empty())
            stream.insert("takion_versions", std::move(versions));
        stream.insert("packet_loss_max", static_cast<double>(packetLossMax));
        config.insert("stream", std::move(stream));
    }

    {
        toml::table ui;
        ui.insert("theme", uiTheme);
        if (hideAccountName) ui.insert("hide_account_name", true);
        ui.insert("connection_show_stages", connectionShowStages);
        config.insert("ui", std::move(ui));
    }

    {
        toml::table updates;
        updates.insert("channel", updateChannel);
        updates.insert("auto_check", autoCheckUpdates);
        if (lastUpdateCheck != 0) updates.insert("last_check", lastUpdateCheck);
        if (!updateInstallPath.empty()) updates.insert("install_path", updateInstallPath);
        config.insert("updates", std::move(updates));
    }

    {
        toml::table debug;
        if (!debugLocale.empty()) debug.insert("locale", debugLocale);
        if (enableFileLogging) debug.insert("file_logging", true);
        if (silenceAllLogs) debug.insert("silence_all_logs", true);
        if (debugLwipLog) debug.insert("lwip_log", true);
        if (debugWireguardLog) debug.insert("wireguard_log", true);
        if (debugRenderLog) debug.insert("render_log", true);
        if (chiakiLogVerbosity != ChiakiLogVerbosity::Normal)
            debug.insert("chiaki_log_level", chiakiLogVerbosityToString(chiakiLogVerbosity));
        if (debugDiscoveryLog) debug.insert("discovery_log", true);
        if (debugFfmpegLog) debug.insert("ffmpeg_log", true);
        if (ipcStatsEnabled) debug.insert("ipc_stats", true);
        if (devFakeHosts) debug.insert("fake_hosts", true);
        if (powerUserMenuUnlocked) debug.insert("power_user_menu_unlocked", true);
        if (unlockBitrateMax) debug.insert("unlock_bitrate_max", true);
        if (!devUpdateServer.empty()) debug.insert("update_server", devUpdateServer);
        if (!debug.empty())
            config.insert("debug", std::move(debug));
    }

    {
        toml::table input;
        input.insert("haptic", std::to_underlying(globalHaptic));
        input.insert("gyro_source", std::to_underlying(globalGyroSource));

        toml::table rumble;
        for (const auto& [key, profile] : rumbleProfiles) {
            toml::table entry;
            entry.insert("strength", static_cast<double>(profile.strength));
            entry.insert("ceiling", static_cast<double>(profile.ceiling));
            entry.insert("per_motor", profile.per_motor);

            if (!profile.per_motor) {
                entry.insert("freq_low", static_cast<double>(profile.freq_low));
                entry.insert("freq_high", static_cast<double>(profile.freq_high));
                entry.insert("envelope_attack", static_cast<double>(profile.envelope_attack));
                entry.insert("envelope_decay", static_cast<double>(profile.envelope_decay));
            }

            entry.insert("haptic_intensity",
                         static_cast<int64_t>(std::to_underlying(profile.haptic_intensity)));
            entry.insert("output_mode",
                         static_cast<int64_t>(std::to_underlying(profile.output_mode)));
            entry.insert("lightbar_enabled", profile.lightbar_enabled);
            entry.insert("lightbar_r", (int64_t)profile.lightbar_r);
            entry.insert("lightbar_g", (int64_t)profile.lightbar_g);
            entry.insert("lightbar_b", (int64_t)profile.lightbar_b);
            entry.insert("rumble_source",
                         static_cast<int64_t>(std::to_underlying(profile.rumble_source)));

            rumble.insert(key, std::move(entry));
        }
        input.insert("rumble", std::move(rumble));

        toml::table mapping;
        if (buttonMapping != getDefaultButtonMapping()) {
            for (const auto& [chiakiBtn, combo] : buttonMapping) {
                std::string key = chiakiButtonToConfigKey(chiakiBtn);
                if (key.empty()) continue;

                toml::array arr;
                for (uint64_t hidBtn : combo) {
                    std::string btnName = hidButtonToConfigString(hidBtn);
                    if (!btnName.empty())
                        arr.push_back(btnName);
                }
                mapping.insert(key, arr);
            }
        }

        mapping.insert("touchpad_enabled", touchpadEnabled);
        mapping.insert("swipe_up_enabled", swipeUpEnabled);
        mapping.insert("swipe_down_enabled", swipeDownEnabled);
        mapping.insert("swipe_left_enabled", swipeLeftEnabled);
        mapping.insert("swipe_right_enabled", swipeRightEnabled);
        input.insert("button_mapping", std::move(mapping));

        config.insert("input", std::move(input));
    }

    {
        std::lock_guard<std::recursive_mutex> profileLock(profileMutex);
        toml::array profilesArr;
        for (const Profile& p : profiles) {
            toml::table pt;
            pt.insert("profile_id", p.id);
            if (!p.onlineId.empty()) pt.insert("online_id", p.onlineId);
            if (!p.accountId.empty()) pt.insert("account_id", p.accountId);
            if (!p.avatarUrl.empty()) pt.insert("avatar_url", p.avatarUrl);
            if (!p.trophiesEnabled) pt.insert("trophies_enabled", false);
            if (p.legacy) pt.insert("legacy", true);
            if (!p.cloudShortcuts.empty()) pt.insert("cloud_shortcuts", shortcutsToToml(p.cloudShortcuts));
            profilesArr.push_back(pt);
        }
        if (!profilesArr.empty())
            config.insert("profiles", profilesArr);

        if (activeProfileId > 0)
            config.insert("active_profile_id", activeProfileId);
    }

    {
        toml::array consolesArr;
        toml::array registrationsArr;

        for (const auto& [name, host] : hosts) {
            host->inConfig = true;

            toml::table ct;
            ct.insert("console_id", host->consoleId);
            ct.insert("nickname", name);
            if (!host->serverNickname.empty()) ct.insert("server_nickname", host->serverNickname);
            ct.insert("host_addr", host->getHostAddr());
            ct.insert("target", targetOf(host->getFirmware()));
            if (host->hardwareId) ct.insert("hardware_id", host->hardwareId->text());
            ct.insert("host_type", std::to_underlying(host->hostType));
            if (!host->consolePIN.empty()) ct.insert("console_pin", host->consolePIN);
            if (!host->remoteDuid.empty()) ct.insert("remote_duid", host->remoteDuid);
            if (host->haptic >= 0) ct.insert("haptic", host->haptic);
            consolesArr.push_back(ct);

            for (const Registration& reg : host->legacyRegistrations) {
                toml::table rt;
                rt.insert("console_id", host->consoleId);
                rt.insert("profile_id", reg.profileId);
                rt.insert("rp_key", akira::base64::encode(reg.keys.remote_play_key()));
                rt.insert("rp_regist_key", akira::base64::encode(reg.keys.registration_key()));
                rt.insert("rp_key_type", static_cast<int64_t>(reg.keys.key_type()));
                registrationsArr.push_back(rt);
            }
        }

        if (!consolesArr.empty())
            config.insert("consoles", consolesArr);
        if (!registrationsArr.empty())
            config.insert("registrations", registrationsArr);
    }

    return config;
}

int SettingsManager::persist(toml::table config) {
    {
        toml::table datacenters;
        std::lock_guard<std::mutex> datacenterLock(datacenterMutex);
        if (!cloudDatacentersPscloud.empty())
            datacenters.insert("pscloud", datacentersToToml(cloudDatacentersPscloud));
        if (!cloudDatacentersPsnow.empty())
            datacenters.insert("psnow", datacentersToToml(cloudDatacentersPsnow));
        if (!datacenters.empty()) {
            if (auto* cloud = config["cloud"].as_table()) {
                cloud->insert_or_assign("datacenters", std::move(datacenters));
            } else {
                toml::table cloudTable;
                cloudTable.insert("datacenters", std::move(datacenters));
                config.insert("cloud", std::move(cloudTable));
            }
        }
    }

    signInStore.write(config);
    pairingStore.write(config);

    std::string tmpPath = std::format("{}.{}.tmp", TOML_CONFIG_FILE, writeSeq++);
    {
        std::ofstream configFile(tmpPath, std::ios::out | std::ios::trunc);
        if (!configFile.is_open()) {
            brls::Logger::error("Failed to open config file for writing");
            return -1;
        }
        configFile << config;
        configFile.flush();
        configFile.close();
        if (configFile.fail()) {
            brls::Logger::error("Failed to flush config to {}", tmpPath);
            std::remove(tmpPath.c_str());
            return -1;
        }
    }

    std::remove(TOML_CONFIG_FILE);
    if (std::rename(tmpPath.c_str(), TOML_CONFIG_FILE) != 0) {
        brls::Logger::error("Failed to move config into place");
        std::remove(tmpPath.c_str());
        return -1;
    }
    return 0;
}

std::string SettingsManager::resolutionToString(VideoResolution resolution) {
    switch (resolution) {
        case VideoResolution::P360: return "360p";
        case VideoResolution::P540: return "540p";
        case VideoResolution::P720: return "720p";
        case VideoResolution::P1080: return "1080p";
        case VideoResolution::P1440: return "1440p";
        case VideoResolution::P2160: return "2160p";
        default: return "720p";
    }
}

int SettingsManager::resolutionToInt(VideoResolution resolution) {
    switch (resolution) {
        case VideoResolution::P360: return 360;
        case VideoResolution::P540: return 540;
        case VideoResolution::P720: return 720;
        case VideoResolution::P1080: return 1080;
        case VideoResolution::P1440: return 1440;
        case VideoResolution::P2160: return 2160;
        default: return 720;
    }
}

VideoResolution SettingsManager::intToResolution(int height) {
    if (height >= 2160) return VideoResolution::P2160;
    if (height >= 1440) return VideoResolution::P1440;
    if (height >= 1080) return VideoResolution::P1080;
    if (height >= 720) return VideoResolution::P720;
    if (height >= 540) return VideoResolution::P540;
    return VideoResolution::P360;
}

VideoResolution SettingsManager::stringToResolution(const std::string& value) {
    if (value == "2160p") return VideoResolution::P2160;
    if (value == "1440p") return VideoResolution::P1440;
    if (value == "1080p") return VideoResolution::P1080;
    if (value == "720p") return VideoResolution::P720;
    if (value == "540p") return VideoResolution::P540;
    if (value == "360p") return VideoResolution::P360;
    return VideoResolution::P720;
}

// FPS helpers
std::string SettingsManager::fpsToString(VideoFps fps) {
    switch (fps) {
        case VideoFps::Fps30: return "30";
        case VideoFps::Fps60: return "60";
        default: return "60";
    }
}

int SettingsManager::fpsToInt(VideoFps fps) {
    switch (fps) {
        case VideoFps::Fps30: return 30;
        case VideoFps::Fps60: return 60;
        default: return 60;
    }
}

VideoFps SettingsManager::stringToFps(const std::string& value) {
    if (value == "60") return VideoFps::Fps60;
    if (value == "30") return VideoFps::Fps30;
    return VideoFps::Fps60;
}

int SettingsManager::getDefaultBitrateForResolution(VideoResolution res) {
    switch (res) {
        case VideoResolution::P2160: return 50000;
        case VideoResolution::P1440: return 30000;
        case VideoResolution::P1080: return 15000;
        case VideoResolution::P720: return 10000;
        case VideoResolution::P540: return 5000;
        case VideoResolution::P360: return 2000;
        default: return 10000;
    }
}

int SettingsManager::getMaxBitrateForResolution(VideoResolution res) const {
    if (unlockBitrateMax) {
        switch (res) {
            case VideoResolution::P2160: return 120000;
            case VideoResolution::P1440: return 80000;
            case VideoResolution::P1080: return 50000;
            case VideoResolution::P720: return 40000;
            case VideoResolution::P540: return 10000;
            case VideoResolution::P360: return 5000;
            default: return 40000;
        }
    }
    switch (res) {
        case VideoResolution::P2160: return 60000;
        case VideoResolution::P1440: return 40000;
        case VideoResolution::P1080: return 25000;
        case VideoResolution::P720: return 20000;
        case VideoResolution::P540: return 10000;
        case VideoResolution::P360: return 5000;
        default: return 20000;
    }
}

bool SettingsManager::isValidIPv4(const std::string& addr) {
    static std::regex ipv4Regex(R"(^(\d+)\.(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;
    if (!std::regex_match(addr, match, ipv4Regex)) {
        return false;
    }
    for (int i = 1; i <= 4; i++) {
        int octet = std::stoi(match[i].str());
        if (octet > 255) {
            return false;
        }
    }
    return true;
}

bool SettingsManager::isValidFQDN(const std::string& addr) {
    if (addr.empty() || addr.length() > 253) {
        return false;
    }
    if (addr.front() == '.' || addr.back() == '.') {
        return false;
    }
    if (addr.front() == '-' || addr.back() == '-') {
        return false;
    }
    // TLDs must contain at least one letter - reject pure numeric strings like "192.168.50.266"
    bool hasAlpha = std::ranges::any_of(addr, [](unsigned char c) { return std::isalpha(c); });
    if (!hasAlpha) {
        return false;
    }
    static std::regex fqdnRegex(R"(^[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+$)");
    return std::regex_match(addr, fqdnRegex);
}

bool SettingsManager::isValidHostAddress(const std::string& addr) {
    return isValidIPv4(addr) || isValidFQDN(addr);
}

std::string SettingsManager::getHostName(Host* host) {
    if (host) return host->getHostName();
    brls::Logger::error("Cannot getHostName from nullptr");
    return "";
}

std::string SettingsManager::getHostAddr(Host* host) {
    if (host) return host->getHostAddr();
    brls::Logger::error("Cannot getHostAddr from nullptr");
    return "";
}

void SettingsManager::setHostAddr(Host* host, const std::string& addr) {
    if (host) {
        host->hostAddr = addr;
    } else {
        brls::Logger::error("Cannot setHostAddr on nullptr");
    }
}

void SettingsManager::setDiscovered(Host* host, bool value) {
    if (host) {
        host->discovered = value;
    }
}

std::string SettingsManager::getPsnOnlineId(Host* host) {
    (void)host;
    const Profile* ap = getActiveProfile();
    return ap ? ap->onlineId : "";
}

void SettingsManager::setPsnOnlineId(Host* host, const std::string& id) {
    (void)host;
    ensureActiveProfile()->onlineId = id;
}

std::string SettingsManager::getPsnAccountId(Host* host) {
    (void)host;
    const Profile* ap = getActiveProfile();
    return ap ? ap->accountId : "";
}

void SettingsManager::setPsnAccountId(Host* host, const std::string& id) {
    (void)host;
    ensureActiveProfile()->accountId = id;
}

std::string SettingsManager::getConsolePIN(Host* host) {
    if (host) return host->consolePIN;
    return "";
}

void SettingsManager::setConsolePIN(Host* host, const std::string& pin) {
    if (host) {
        host->consolePIN = pin;
    } else {
        brls::Logger::error("Cannot setConsolePIN on nullptr");
    }
}

VideoResolution SettingsManager::getVideoResolution(Host* host) {
    if (host && host->isRemote()) return remoteVideoResolution;
    return localVideoResolution;
}

VideoResolution SettingsManager::getLocalVideoResolution() const {
    return localVideoResolution;
}

void SettingsManager::setLocalVideoResolution(VideoResolution value) {
    localVideoResolution = value;
}

VideoResolution SettingsManager::getRemoteVideoResolution() const {
    return remoteVideoResolution;
}

void SettingsManager::setRemoteVideoResolution(VideoResolution value) {
    remoteVideoResolution = value;
}

VideoFps SettingsManager::getVideoFPS(Host* host) {
    if (host && host->isRemote()) return remoteVideoFPS;
    return localVideoFPS;
}

VideoFps SettingsManager::getLocalVideoFPS() const {
    return localVideoFPS;
}

void SettingsManager::setLocalVideoFPS(VideoFps value) {
    localVideoFPS = value;
}

VideoFps SettingsManager::getRemoteVideoFPS() const {
    return remoteVideoFPS;
}

void SettingsManager::setRemoteVideoFPS(VideoFps value) {
    remoteVideoFPS = value;
}

int SettingsManager::getVideoBitrate(Host* host) const {
    if (host && host->isRemote()) return remoteVideoBitrate;
    return localVideoBitrate;
}

int SettingsManager::getLocalVideoBitrate() const {
    return localVideoBitrate;
}

void SettingsManager::setLocalVideoBitrate(int value) {
    localVideoBitrate = value;
}

int SettingsManager::getRemoteVideoBitrate() const {
    return remoteVideoBitrate;
}

void SettingsManager::setRemoteVideoBitrate(int value) {
    remoteVideoBitrate = value;
}

int SettingsManager::getVpnVideoBitrate() const {
    return vpnVideoBitrate;
}

void SettingsManager::setVpnVideoBitrate(int value) {
    vpnVideoBitrate = value;
}

int SettingsManager::getCloudVideoBitrate(bool pscloud) const {
    return pscloud ? cloudVideoBitratePscloud : cloudVideoBitratePsnow;
}

void SettingsManager::setCloudVideoBitrate(bool pscloud, int value) {
    if (pscloud)
        cloudVideoBitratePscloud = value;
    else
        cloudVideoBitratePsnow = value;
}

int SettingsManager::getCloudVideoResolution(bool pscloud) const {
    return pscloud ? cloudVideoResolutionPscloud : cloudVideoResolutionPsnow;
}

void SettingsManager::setCloudVideoResolution(bool pscloud, int value) {
    if (pscloud)
        cloudVideoResolutionPscloud = value;
    else
        cloudVideoResolutionPsnow = value;
}

bool SettingsManager::getCloudFsrEnabled(bool pscloud) const {
    return pscloud ? cloudFsrEnabledPscloud : cloudFsrEnabledPsnow;
}

void SettingsManager::setCloudFsrEnabled(bool pscloud, bool enabled) {
    if (pscloud)
        cloudFsrEnabledPscloud = enabled;
    else
        cloudFsrEnabledPsnow = enabled;
}

std::vector<cloud::Datacenter> SettingsManager::getCloudDatacenters(bool pscloud) const {
    std::lock_guard<std::mutex> lock(datacenterMutex);
    return pscloud ? cloudDatacentersPscloud : cloudDatacentersPsnow;
}

void SettingsManager::setCloudDatacenters(bool pscloud, std::vector<cloud::Datacenter> datacenters) {
    std::lock_guard<std::mutex> lock(datacenterMutex);
    if (pscloud)
        cloudDatacentersPscloud = std::move(datacenters);
    else
        cloudDatacentersPsnow = std::move(datacenters);
}

int SettingsManager::getCloudSortState() const {
    return cloudSortState;
}

void SettingsManager::setCloudSortState(int value) {
    cloudSortState = value;
}

const std::string& SettingsManager::getCloudFilter() const {
    return cloudFilter;
}

void SettingsManager::setCloudFilter(std::string filter) {
    cloudFilter = std::move(filter);
}

const std::vector<cloud::Game>& SettingsManager::getCloudShortcuts() const {
    static const std::vector<cloud::Game> none;
    const Profile* p = getActiveProfile();
    return p ? p->cloudShortcuts : none;
}

void SettingsManager::setCloudShortcuts(std::vector<cloud::Game> shortcuts) {
    ensureActiveProfile()->cloudShortcuts = std::move(shortcuts);
}

VideoResolution SettingsManager::getVpnVideoResolution() const {
    return vpnVideoResolution;
}

void SettingsManager::setVpnVideoResolution(VideoResolution value) {
    vpnVideoResolution = value;
}

VideoFps SettingsManager::getVpnVideoFPS() const {
    return vpnVideoFPS;
}

void SettingsManager::setVpnVideoFPS(VideoFps value) {
    vpnVideoFPS = value;
}

HapticPreset SettingsManager::getHaptic(Host* host) {
    if (!host) return globalHaptic;
    if (host->haptic < 0) return globalHaptic;
    switch (host->haptic) {
        case 0: return HapticPreset::Disabled;
        case 1: return HapticPreset::Weak;
        case 2: return HapticPreset::Strong;
        default: return globalHaptic;
    }
}

void SettingsManager::setHaptic(Host* host, HapticPreset value) {
    if (host) {
        host->haptic = static_cast<int>(value);
    } else {
        globalHaptic = value;
    }
}

void SettingsManager::setHaptic(Host* host, const std::string& value) {
    HapticPreset preset = HapticPreset::Disabled;
    if (value == "1") preset = HapticPreset::Weak;
    else if (value == "2") preset = HapticPreset::Strong;
    setHaptic(host, preset);
}

std::string SettingsManager::resolveRumbleKey(uint16_t vendorId, uint16_t productId,
                                              const uint8_t* address, bool switchNative,
                                              bool joycon) const
{

    if (address != nullptr) {
        const std::string unit = akira::input::RumbleKeyForUnit(address);
        if (rumbleProfiles.find(unit) != rumbleProfiles.end())
            return unit;
    }

    if (vendorId != 0 || productId != 0) {
        const std::string model = akira::input::RumbleKeyForModel(vendorId, productId);
        if (rumbleProfiles.find(model) != rumbleProfiles.end())
            return model;
    }

    const std::string category =
        akira::input::RumbleKeyForCategory(
            akira::input::PadCategoryFor(vendorId, productId, switchNative, joycon));
    if (rumbleProfiles.find(category) != rumbleProfiles.end())
        return category;

    return akira::input::kRumbleKeyDefault;
}

akira::input::RumbleProfile SettingsManager::resolveRumbleProfile(uint16_t vendorId, uint16_t productId,
                                                                  const uint8_t* address,
                                                                  bool switchNative,
                                                                  bool joycon) const
{
    return getRumbleProfile(resolveRumbleKey(vendorId, productId, address, switchNative, joycon));
}

akira::input::RumbleProfile SettingsManager::getRumbleProfile(const std::string& key) const
{
    const auto it = rumbleProfiles.find(key);
    if (it != rumbleProfiles.end())
        return it->second;

    return key == akira::input::kRumbleKeySwitch
               ? akira::input::SwitchRumbleProfile()
               : akira::input::DefaultRumbleProfile();
}

bool SettingsManager::hasRumbleProfile(const std::string& key) const
{
    return rumbleProfiles.find(key) != rumbleProfiles.end();
}

void SettingsManager::setRumbleProfile(const std::string& key, const akira::input::RumbleProfile& profile)
{
    if (key.empty())
        return;

    rumbleProfiles[key] = profile;
}

bool SettingsManager::seedRumbleProfile(const std::string& key,
                                        const akira::input::RumbleProfile* inheritFrom)
{
    if (key.empty() || hasRumbleProfile(key))
        return false;

    if (inheritFrom != nullptr) {
        rumbleProfiles[key] = *inheritFrom;
        return true;
    }

    rumbleProfiles[key] = key == akira::input::kRumbleKeySwitch
                              ? akira::input::SwitchRumbleProfile()
                              : akira::input::DefaultRumbleProfile();
    return true;
}

void SettingsManager::resetRumbleProfile(const std::string& key)
{
    if (key.empty())
        return;

    rumbleProfiles[key] = key == akira::input::kRumbleKeySwitch
                              ? akira::input::SwitchRumbleProfile()
                              : akira::input::DefaultRumbleProfile();
}

bool SettingsManager::getDirectRumbleInStream() const { return directRumbleInStream; }
void SettingsManager::setDirectRumbleInStream(bool value) { directRumbleInStream = value; }
bool SettingsManager::getDirectHapticsInStream() const { return directHapticsInStream; }
void SettingsManager::setDirectHapticsInStream(bool value) { directHapticsInStream = value; }


std::optional<libchiaking::consoles::Firmware> SettingsManager::firmwareFromTarget(int64_t target) {
    using Family = libchiaking::consoles::Firmware::Family;
    if (target >= 1000000)
        return libchiaking::consoles::Firmware{Family::ps5_1};
    if (target >= 1000)
        return libchiaking::consoles::Firmware{Family::ps4_10};
    if (target >= 900)
        return libchiaking::consoles::Firmware{Family::ps4_9};
    if (target >= 800)
        return libchiaking::consoles::Firmware{Family::ps4_8};
    return std::nullopt;
}

int64_t SettingsManager::targetOf(const std::optional<libchiaking::consoles::Firmware>& firmware) {
    using Family = libchiaking::consoles::Firmware::Family;
    if (!firmware)
        return 0;
    switch (firmware->family()) {
        case Family::ps4_8: return 800;
        case Family::ps4_9: return 900;
        case Family::ps4_10: return 1000;
        case Family::ps5_1: return 1000100;
    }
    return 0;
}

std::optional<Registration> SettingsManager::registrationOf(const toml::table& row) {
    std::array<std::byte, libchiaking::consoles::PairingKeys::registration_key_size> registKey{};
    std::array<std::byte, libchiaking::consoles::PairingKeys::remote_play_key_size> key{};
    const bool decoded =
        akira::base64::decodeExactly(row["rp_regist_key"].value_or(std::string_view{}), registKey) &&
        akira::base64::decodeExactly(row["rp_key"].value_or(std::string_view{}), key);
    auto keys = decoded
        ? libchiaking::consoles::PairingKeys::create(registKey, key,
              static_cast<uint32_t>(row["rp_key_type"].value_or(int64_t{0})))
        : std::nullopt;
    libchiaking::memory::secure_zero(std::span<std::byte>{registKey});
    libchiaking::memory::secure_zero(std::span<std::byte>{key});
    if (!keys)
        return std::nullopt;
    return Registration{row["profile_id"].value_or(int64_t{0}), std::move(*keys)};
}

bool SettingsManager::adoptLegacyRegistrations(Host& host) {
    namespace consoles = libchiaking::consoles;
    if (!host.hardwareId || host.legacyRegistrations.empty())
        return false;
    const auto firmware = host.getFirmware();
    if (!firmware)
        return false;

    const std::string name = host.serverNickname.empty() ? host.hostName : host.serverNickname;
    consoles::SavedConsole console{*host.hardwareId, *firmware,
        consoles::ConsoleName::create(name, consoles::ConsoleName::Source::pairing)
            .value_or(consoles::ConsoleName::of(firmware->generation()))};
    if (auto address = consoles::Host::create(host.getHostAddr(), firmware->generation()))
        console.last_address = std::move(address.value());

    std::vector<consoles::SavedPairing> adopted;
    {
        std::lock_guard<std::recursive_mutex> lock(profileMutex);
        for (const Registration& reg : host.legacyRegistrations) {
            const Profile* profile = findProfile(reg.profileId);
            const auto account = profile ? libchiaking::AccountId::from_base64(profile->accountId) : std::nullopt;
            if (!account)
                continue;
            adopted.push_back(consoles::SavedPairing{*account, *host.hardwareId, reg.keys.clone(),
                consoles::DeviceUid::parse(host.remoteDuid)});
        }
    }
    if (adopted.empty())
        return false;

    if (!pairingStore.adopt(std::move(console), std::move(adopted))) {
        brls::Logger::error("Could not adopt the saved registrations of {}", host.hostName);
        return false;
    }
    host.legacyRegistrations.clear();
    writeFile();
    brls::Logger::info("Adopted the saved registrations of {} into the core's pairings", host.hostName);
    return true;
}

void SettingsManager::backupConfig(int64_t version) {
    const std::string backupPath = std::format("{}.v{}.bak", TOML_CONFIG_FILE, version);
    std::ifstream source(TOML_CONFIG_FILE, std::ios::binary);
    std::ofstream backup(backupPath, std::ios::binary | std::ios::trunc);
    if (source && backup)
        backup << source.rdbuf();
    backup.flush();
    const auto written = backup.good() ? static_cast<std::streamoff>(backup.tellp()) : std::streamoff{-1};
    std::error_code error;
    const auto expected = std::filesystem::file_size(TOML_CONFIG_FILE, error);
    if (error || written < 0 || static_cast<std::uintmax_t>(written) != expected)
        brls::Logger::error("Could not back up {} to {} before migrating", TOML_CONFIG_FILE, backupPath);
    else
        brls::Logger::info("Backed up {} to {} before migrating", TOML_CONFIG_FILE, backupPath);
}

int SettingsManager::getCompanionPort() const {
    return companionPort;
}

void SettingsManager::setCompanionPort(int port) {
    if (port > 0 && port <= 65535) {
        companionPort = port;
    }
}

const std::vector<Profile>& SettingsManager::getProfiles() const {
    return profiles;
}

std::vector<Profile> SettingsManager::profilesSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(profileMutex);
    return profiles;
}

Profile* SettingsManager::findProfile(int64_t id) {
    for (Profile& p : profiles)
        if (p.id == id) return &p;
    return nullptr;
}

const Profile* SettingsManager::findProfile(int64_t id) const {
    for (const Profile& p : profiles)
        if (p.id == id) return &p;
    return nullptr;
}

Profile* SettingsManager::getActiveProfile() {
    return findProfile(activeProfileId);
}

const Profile* SettingsManager::getActiveProfile() const {
    return findProfile(activeProfileId);
}

Profile* SettingsManager::ensureActiveProfile() {
    if (Profile* p = getActiveProfile())
        return p;
    Profile np;
    int64_t id = addProfile(np);
    activeProfileId = id;
    return findProfile(id);
}

int64_t SettingsManager::getActiveProfileId() const {
    return activeProfileId;
}

void SettingsManager::setActiveProfileId(int64_t id) {
    if (activeProfileId != id) {
        const Profile* from = findProfile(activeProfileId);
        const Profile* to = findProfile(id);
        brls::Logger::info("Active profile {} ({}) -> {} ({})",
            activeProfileId, from ? from->label() : std::string("<none>"),
            id, to ? to->label() : std::string("<none>"));
    }
    activeProfileId = id;
    refreshLegacyGate();
}

void SettingsManager::logProfileCensus(const char* stage) {
    brls::Logger::info("Profile census [{}]: active={} count={}",
        stage, activeProfileId, profiles.size());
    for (const Profile& p : profiles) {
        brls::Logger::info("  profile id={} label='{}' legacy={} account={} shortcuts={}",
            p.id, p.label(), p.legacy, !p.accountId.empty(), p.cloudShortcuts.size());
    }
}

bool SettingsManager::getActiveProfileTrophiesEnabled() const {
    const Profile* p = getActiveProfile();
    return p ? p->trophiesEnabled : true;
}

void SettingsManager::setActiveProfileTrophiesEnabled(bool enabled) {
    Profile* p = getActiveProfile();
    if (p)
        p->trophiesEnabled = enabled;
}

int64_t SettingsManager::addProfile(const Profile& profile) {
    std::lock_guard<std::recursive_mutex> lock(profileMutex);
    Profile copy = profile;
    copy.id = nextProfileId++;
    profiles.push_back(copy);
    return copy.id;
}

void SettingsManager::removeProfile(int64_t id) {
    std::lock_guard<std::recursive_mutex> lock(profileMutex);
    std::erase_if(profiles, [id](const Profile& p) { return p.id == id; });
    if (activeProfileId == id)
        activeProfileId = profiles.empty() ? 0 : profiles.front().id;

    /*
     * Registrations are keyed by profile, so dropping the profile without them left the
     * rp keys behind forever. Deleting and re-pairing is the sanctioned way to change a
     * profile's mode, which makes this the common path rather than a rare one.
     */
    for (auto& [name, host] : hosts) {
        if (host)
            host->removeRegistrationsForProfile(id);
    }

    refreshLegacyGate();
}

bool SettingsManager::isLegacyProfileActive() const {
    const Profile* p = getActiveProfile();
    return p && p->legacy;
}

void SettingsManager::refreshLegacyGate() {
    httpSetSonyBlocked(isLegacyProfileActive());
}

bool SettingsManager::readProfile(int64_t id,
                                  const std::function<void(const Profile&)>& fn) const {
    std::lock_guard<std::recursive_mutex> lock(profileMutex);
    const Profile* p = findProfile(id);
    if (!p)
        return false;
    fn(*p);
    return true;
}

bool SettingsManager::updateProfile(int64_t id,
                                    const std::function<void(Profile&)>& fn) {
    std::lock_guard<std::recursive_mutex> lock(profileMutex);
    Profile* p = findProfile(id);
    if (!p)
        return false;
    fn(*p);
    return true;
}

bool SettingsManager::getHolepunchRetry() const {
    return holepunchRetry;
}

void SettingsManager::setHolepunchRetry(bool retry) {
    holepunchRetry = retry;
}

bool SettingsManager::getConnectionShowStages() const {
    return connectionShowStages;
}

void SettingsManager::setConnectionShowStages(bool show) {
    connectionShowStages = show;
}

bool SettingsManager::getPortGuessing() const {
    return portGuessing;
}

void SettingsManager::setPortGuessing(bool enabled) {
    portGuessing = enabled;
}

int SettingsManager::getPortGuessingCount() const {
    return portGuessingCount;
}

void SettingsManager::setPortGuessingCount(int count) {
    if (count > 0)
        portGuessingCount = count;
}

int SettingsManager::getPortGuessingSocks() const {
    return portGuessingSocks;
}

void SettingsManager::setPortGuessingSocks(int count) {
    if (count > 0)
        portGuessingSocks = count;
}

bool SettingsManager::getPowerUserMenuUnlocked() const {
    return powerUserMenuUnlocked;
}

void SettingsManager::setPowerUserMenuUnlocked(bool unlocked) {
    powerUserMenuUnlocked = unlocked;
}

bool SettingsManager::getIpcStatsEnabled() const {
    return ipcStatsEnabled;
}

void SettingsManager::setIpcStatsEnabled(bool enabled) {
    ipcStatsEnabled = enabled;
}

bool SettingsManager::getUnlockBitrateMax() const {
    return unlockBitrateMax;
}

void SettingsManager::setUnlockBitrateMax(bool enabled) {
    unlockBitrateMax = enabled;
}

bool SettingsManager::getAutoReconnect() const {
    return autoReconnect;
}

void SettingsManager::setAutoReconnect(bool enabled) {
    autoReconnect = enabled;
}

bool SettingsManager::getDevFakeHosts() const {
    return devFakeHosts;
}

bool SettingsManager::getHideAccountName() const {
    return hideAccountName;
}

void SettingsManager::setHideAccountName(bool enabled) {
    hideAccountName = enabled;
}

std::string SettingsManager::maskAccountName(const std::string& name) const {
    if (!hideAccountName || name.size() <= 2)
        return name;
    return name.substr(0, 2) + "***";
}

void SettingsManager::setDevFakeHosts(bool enabled) {
    devFakeHosts = enabled;
}

std::string SettingsManager::getUpdateChannel() const {
    return updateChannel;
}

void SettingsManager::setUpdateChannel(const std::string& channel) {
    updateChannel = channel;
}

std::string SettingsManager::getUiTheme() const {
    return uiTheme;
}

void SettingsManager::setUiTheme(const std::string& id) {
    uiTheme = id;
}

std::string SettingsManager::getDiscoverySubnets() const {
    return discoverySubnets;
}

void SettingsManager::setDiscoverySubnets(const std::string& subnets) {
    discoverySubnets = subnets;
}

bool SettingsManager::getAutoCheckUpdates() const {
    return autoCheckUpdates;
}

void SettingsManager::setAutoCheckUpdates(bool enabled) {
    autoCheckUpdates = enabled;
}

int64_t SettingsManager::getLastUpdateCheck() const {
    return lastUpdateCheck;
}

void SettingsManager::setLastUpdateCheck(int64_t epochSeconds) {
    lastUpdateCheck = epochSeconds;
}

std::string SettingsManager::getUpdateInstallPath() const {
    return updateInstallPath;
}

void SettingsManager::setUpdateInstallPath(const std::string& path) {
    updateInstallPath = path;
}

std::string SettingsManager::getDevUpdateServer() const {
    return devUpdateServer;
}

void SettingsManager::setDevUpdateServer(const std::string& server) {
    devUpdateServer = server;
}

int SettingsManager::getMinBitrateForResolution(VideoResolution res) const {
    if (unlockBitrateMax) {
        switch (res) {
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

GyroSource SettingsManager::getGyroSource() const {
    return globalGyroSource;
}

void SettingsManager::setGyroSource(GyroSource source) {
    globalGyroSource = source;
}

bool SettingsManager::getSleepOnExit() const {
    return sleepOnExit;
}

void SettingsManager::setSleepOnExit(bool enabled) {
    sleepOnExit = enabled;
}

bool SettingsManager::getRequestIdrOnFecFailure() const {
    return requestIdrOnFecFailure;
}

void SettingsManager::setRequestIdrOnFecFailure(bool enabled) {
    requestIdrOnFecFailure = enabled;
}

int SettingsManager::getTakionVersion(TakionTarget target) const {
    return takionVersions[static_cast<size_t>(target)];
}

void SettingsManager::setTakionVersion(TakionTarget target, int version) {
    takionVersions[static_cast<size_t>(target)] = version;
}

auto SettingsManager::takionTargetKey(TakionTarget target) -> std::string_view {
    switch (target) {
        case TakionTarget::PS4:
            return "ps4";
        case TakionTarget::PS5:
            return "ps5";
        case TakionTarget::PS5Cloud:
            return "ps5_cloud";
        case TakionTarget::PSNow:
            return "ps_now";
    }
    return "ps5";
}

float SettingsManager::getPacketLossMax() const {
    return packetLossMax;
}

void SettingsManager::setPacketLossMax(float value) {
    packetLossMax = value;
}

bool SettingsManager::getEasuEnabled() const {
    switch (activeStreamProfile) {
        case StreamProfile::Remote: return remoteFsrEnabled;
        case StreamProfile::Vpn: return vpnFsrEnabled;
        case StreamProfile::Cloud: return getCloudFsrEnabled(activeCloudPscloud);
        case StreamProfile::Local:
        default: return localFsrEnabled;
    }
}

int SettingsManager::getEasuTargetHeight() const {
    VideoResolution res;
    switch (activeStreamProfile) {
        case StreamProfile::Remote: res = remoteVideoResolution; break;
        case StreamProfile::Vpn: res = vpnVideoResolution; break;
        case StreamProfile::Cloud:
            res = intToResolution(getCloudVideoResolution(activeCloudPscloud));
            break;
        case StreamProfile::Local:
        default: res = localVideoResolution; break;
    }
    return fsrTargetHeightForResolution(res);
}

bool SettingsManager::getLocalFsrEnabled() const {
    return localFsrEnabled;
}

void SettingsManager::setLocalFsrEnabled(bool enabled) {
    localFsrEnabled = enabled;
}

bool SettingsManager::getRemoteFsrEnabled() const {
    return remoteFsrEnabled;
}

void SettingsManager::setRemoteFsrEnabled(bool enabled) {
    remoteFsrEnabled = enabled;
}

bool SettingsManager::getVpnFsrEnabled() const {
    return vpnFsrEnabled;
}

void SettingsManager::setVpnFsrEnabled(bool enabled) {
    vpnFsrEnabled = enabled;
}

bool SettingsManager::getRcasEnabled() const {
    return rcasEnabled;
}

void SettingsManager::setRcasEnabled(bool enabled) {
    rcasEnabled = enabled;
}

float SettingsManager::getRcasSharpness() const {
    return rcasSharpness;
}

void SettingsManager::setRcasSharpness(float sharpness) {
    rcasSharpness = sharpness;
}

SettingsManager::StreamProfile SettingsManager::getActiveStreamProfile() const {
    return activeStreamProfile;
}

void SettingsManager::setActiveStreamProfile(StreamProfile profile) {
    activeStreamProfile = profile;
}

bool SettingsManager::getActiveCloudPscloud() const {
    return activeCloudPscloud;
}

void SettingsManager::setActiveCloudPscloud(bool pscloud) {
    activeCloudPscloud = pscloud;
}

bool SettingsManager::getEnableFileLogging() const {
    return enableFileLogging && !silenceAllLogs;
}

bool SettingsManager::getSilenceAllLogs() const {
    return silenceAllLogs;
}

void SettingsManager::setSilenceAllLogs(bool silenced) {
    silenceAllLogs = silenced;
    applyChiakiLogVerbosity();
}

void SettingsManager::setEnableFileLogging(bool enabled) {
    enableFileLogging = enabled;
}

bool SettingsManager::getEnableDithering() const {
    return enableDithering;
}

void SettingsManager::setEnableDithering(bool enabled) {
    enableDithering = enabled;
}

float SettingsManager::getDitheringStrength() const {
    return ditheringStrength;
}

void SettingsManager::setDitheringStrength(float value) {
    ditheringStrength = std::max(1.0f, std::min(10.0f, value));
}

float SettingsManager::getStatsOverlayX() const {
    return statsOverlayX;
}

float SettingsManager::getStatsOverlayY() const {
    return statsOverlayY;
}

void SettingsManager::setStatsOverlayPosition(float x, float y) {
    statsOverlayX = std::max(0.0f, std::min(1.0f, x));
    statsOverlayY = std::max(0.0f, std::min(1.0f, y));
}

std::string SettingsManager::getDebugLocale() const {
    return debugLocale;
}

void SettingsManager::setDebugLocale(const std::string& locale) {
    debugLocale = locale;
}

bool SettingsManager::getDebugLwipLog() const {
    return debugLwipLog;
}

void SettingsManager::setDebugLwipLog(bool enabled) {
    debugLwipLog = enabled;
}

bool SettingsManager::getDebugWireguardLog() const {
    return debugWireguardLog;
}

void SettingsManager::setDebugWireguardLog(bool enabled) {
    debugWireguardLog = enabled;
}

bool SettingsManager::getDebugRenderLog() const {
    return debugRenderLog;
}

void SettingsManager::setDebugRenderLog(bool enabled) {
    debugRenderLog = enabled;
}

ChiakiLogVerbosity SettingsManager::getChiakiLogVerbosity() const {
    return chiakiLogVerbosity;
}

void SettingsManager::setChiakiLogVerbosity(ChiakiLogVerbosity verbosity) {
    chiakiLogVerbosity = verbosity;
    applyChiakiLogVerbosity();
}

void SettingsManager::applyChiakiLogVerbosity() {
    brls::Logger::setLogLevel(silenceAllLogs
        ? static_cast<brls::LogLevel>(static_cast<int>(brls::LogLevel::LOG_ERROR) - 1)
        : borealisLogLevel(chiakiLogVerbosity));
    updateChiakiLogMask();
}

void SettingsManager::updateChiakiLogMask() {
    if (coreLog)
        coreLog->follow(chiakiLogVerbosity, streamingActive, silenceAllLogs);
}

bool SettingsManager::getDebugChiakiLog() const {
    return chiakiLogVerbosity == ChiakiLogVerbosity::Debug ||
           chiakiLogVerbosity == ChiakiLogVerbosity::Trace;
}

bool SettingsManager::getDebugDiscoveryLog() const {
    return debugDiscoveryLog;
}

void SettingsManager::setDebugDiscoveryLog(bool enabled) {
    debugDiscoveryLog = enabled;
}

bool SettingsManager::getDebugFfmpegLog() const {
    return debugFfmpegLog;
}

void SettingsManager::setDebugFfmpegLog(bool enabled) {
    debugFfmpegLog = enabled;
}

bool SettingsManager::isStreamingActive() const {
    return streamingActive;
}

void SettingsManager::setStreamingActive(bool active) {
    streamingActive = active;
    updateChiakiLogMask();
}

bool SettingsManager::getTouchpadEnabled() const { return touchpadEnabled; }
void SettingsManager::setTouchpadEnabled(bool enabled) { touchpadEnabled = enabled; }

bool SettingsManager::getSwipeUpEnabled() const { return swipeUpEnabled; }
void SettingsManager::setSwipeUpEnabled(bool enabled) { swipeUpEnabled = enabled; }

bool SettingsManager::getSwipeDownEnabled() const { return swipeDownEnabled; }
void SettingsManager::setSwipeDownEnabled(bool enabled) { swipeDownEnabled = enabled; }

bool SettingsManager::getSwipeLeftEnabled() const { return swipeLeftEnabled; }
void SettingsManager::setSwipeLeftEnabled(bool enabled) { swipeLeftEnabled = enabled; }

bool SettingsManager::getSwipeRightEnabled() const { return swipeRightEnabled; }
void SettingsManager::setSwipeRightEnabled(bool enabled) { swipeRightEnabled = enabled; }

bool SettingsManager::isButtonEnabled(uint32_t chiakiButton) const {
    switch (chiakiButton) {
        case remapKey(RemapTarget::Touchpad): return touchpadEnabled;
        case SWIPE_TOUCHPAD_UP: return swipeUpEnabled;
        case SWIPE_TOUCHPAD_DOWN: return swipeDownEnabled;
        case SWIPE_TOUCHPAD_LEFT: return swipeLeftEnabled;
        case SWIPE_TOUCHPAD_RIGHT: return swipeRightEnabled;
        default: return true;
    }
}

const ButtonMapping& SettingsManager::getButtonMapping() const {
    return buttonMapping;
}

void SettingsManager::setButtonMapping(const ButtonMapping& mapping) {
    buttonMapping = mapping;
}

ButtonMapping SettingsManager::getDefaultButtonMapping() const {
    ButtonMapping defaults;
    defaults[remapKey(RemapTarget::Cross)]    = {HidNpadButton_B};
    defaults[remapKey(RemapTarget::Moon)]     = {HidNpadButton_A};
    defaults[remapKey(RemapTarget::Box)]      = {HidNpadButton_Y};
    defaults[remapKey(RemapTarget::Pyramid)]  = {HidNpadButton_X};
    defaults[remapKey(RemapTarget::L1)]       = {HidNpadButton_L};
    defaults[remapKey(RemapTarget::R1)]       = {HidNpadButton_R};
    defaults[remapKey(RemapTarget::L2)] = {HidNpadButton_ZL};
    defaults[remapKey(RemapTarget::R2)] = {HidNpadButton_ZR};
    defaults[remapKey(RemapTarget::L3)]       = {HidNpadButton_StickL};
    defaults[remapKey(RemapTarget::R3)]       = {HidNpadButton_StickR};
    defaults[remapKey(RemapTarget::Options)]  = {HidNpadButton_Plus};
    defaults[remapKey(RemapTarget::Share)]    = {};
    defaults[remapKey(RemapTarget::Touchpad)] = {};
    defaults[remapKey(RemapTarget::Ps)]       = {HidNpadButton_Minus};
    defaults[SWIPE_TOUCHPAD_UP]    = {};
    defaults[SWIPE_TOUCHPAD_DOWN]  = {};
    defaults[SWIPE_TOUCHPAD_LEFT]  = {};
    defaults[SWIPE_TOUCHPAD_RIGHT] = {};
    return defaults;
}

static void pruneOldLogs() {
    static constexpr size_t KEEP = 40;

    DIR* dir = opendir(SettingsManager::LOG_DIR);
    if (!dir)
        return;

    std::vector<std::pair<time_t, std::string>> logFiles;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() <= 4 || name.substr(name.size() - 4) != ".log")
            continue;
        std::string path = std::string(SettingsManager::LOG_DIR) + "/" + name;
        struct stat st;
        time_t mtime = (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;
        logFiles.emplace_back(mtime, std::move(path));
    }
    closedir(dir);

    if (logFiles.size() < KEEP)
        return;

    std::sort(logFiles.begin(), logFiles.end());
    size_t toDelete = logFiles.size() - (KEEP - 1);
    for (size_t i = 0; i < toDelete; i++)
        remove(logFiles[i].second.c_str());
}

std::string SettingsManager::getLogFilePath() {
    mkdir(LOG_DIR, 0755);

    pruneOldLogs();

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    return std::format("{}/{:02}{:02}{:02}_{:02}{:02}{:02}.log",
             LOG_DIR, t->tm_mday, t->tm_mon + 1, t->tm_year % 100,
             t->tm_hour, t->tm_min, t->tm_sec);
}

std::string SettingsManager::getConnectionLogFilePath(const std::string& connType) {
    mkdir(LOG_DIR, 0755);

    pruneOldLogs();

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    return std::format("{}/{:02}{:02}{:02}_{:02}{:02}{:02}_{}.log",
             LOG_DIR, t->tm_mday, t->tm_mon + 1, t->tm_year % 100,
             t->tm_hour, t->tm_min, t->tm_sec, connType);
}
