#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
#include "log/core_log.hpp"

namespace akira::settings {

DeveloperSettings::DeveloperSettings(SettingsStore& store)
    : store(store)
{
    read(store.loaded());
    store.attach(*this);
}

DeveloperSettings::~DeveloperSettings()
{
    store.detach(*this);
}

void DeveloperSettings::read(const toml::table& config)
{
    for (const auto target : targets) {
        if (auto val = config["stream"]["takion_versions"][targetKey(target)].value<int64_t>())
            takionVersions[static_cast<size_t>(target)] = static_cast<int>(*val);
    }

    const auto debug = config["debug"];
    if (auto val = debug["file_logging"].value<bool>())
        enableFileLogging = *val;
    if (auto val = debug["silence_all_logs"].value<bool>())
        silenced = *val;
    if (auto val = debug["lwip_log"].value<bool>())
        debugLwipLog = *val;
    if (auto val = debug["wireguard_log"].value<bool>())
        debugWireguardLog = *val;
    if (auto val = debug["render_log"].value<bool>())
        debugRenderLog = *val;
    if (auto val = debug["chiaki_log_level"].value<std::string>())
        verbosity = verbosityOf(*val);
    else if (auto val = debug["chiaki_log"].value<bool>())
        verbosity = *val ? ChiakiLogVerbosity::Trace : ChiakiLogVerbosity::Normal;
    if (auto val = debug["discovery_log"].value<bool>())
        debugDiscoveryLog = *val;
    if (auto val = debug["ffmpeg_log"].value<bool>())
        debugFfmpegLog = *val;
    if (auto val = debug["ipc_stats"].value<bool>())
        ipcStats = *val;
    if (auto val = debug["fake_hosts"].value<bool>())
        devFakeHosts = *val;
    if (auto val = debug["power_user_menu_unlocked"].value<bool>())
        powerUserUnlocked = *val;
    if (auto val = debug["update_server"].value<std::string>())
        devUpdateServer = *val;
}

void DeveloperSettings::write(toml::table& config) const
{
    toml::table versions;
    for (const auto target : targets) {
        if (const int version = takionVersion(target))
            versions.insert(targetKey(target), static_cast<int64_t>(version));
    }
    if (!versions.empty())
        SettingsStore::table(config, "stream").insert("takion_versions", std::move(versions));

    auto debug = [&config]() -> toml::table& { return SettingsStore::table(config, "debug"); };
    if (enableFileLogging) debug().insert("file_logging", true);
    if (silenced) debug().insert("silence_all_logs", true);
    if (debugLwipLog) debug().insert("lwip_log", true);
    if (debugWireguardLog) debug().insert("wireguard_log", true);
    if (debugRenderLog) debug().insert("render_log", true);
    if (verbosity != ChiakiLogVerbosity::Normal)
        debug().insert("chiaki_log_level", textOf(verbosity));
    if (debugDiscoveryLog) debug().insert("discovery_log", true);
    if (debugFfmpegLog) debug().insert("ffmpeg_log", true);
    if (ipcStats) debug().insert("ipc_stats", true);
    if (devFakeHosts) debug().insert("fake_hosts", true);
    if (powerUserUnlocked) debug().insert("power_user_menu_unlocked", true);
    if (!devUpdateServer.empty()) debug().insert("update_server", devUpdateServer);
}

int DeveloperSettings::takionVersion(TakionTarget target) const
{
    return takionVersions[static_cast<size_t>(target)];
}

void DeveloperSettings::setTakionVersion(TakionTarget target, int version)
{
    takionVersions[static_cast<size_t>(target)] = version;
}

void DeveloperSettings::setSilenceAllLogs(bool silence)
{
    silenced = silence;
    applyChiakiLogVerbosity();
}

void DeveloperSettings::setChiakiLogVerbosity(ChiakiLogVerbosity value)
{
    verbosity = value;
    applyChiakiLogVerbosity();
}

void DeveloperSettings::applyChiakiLogVerbosity()
{
    if (silenced)
        log::Log::process().silence();
    else
        log::Log::process().setLevel(appLevel(verbosity));
    updateCoreLog();
}

bool DeveloperSettings::chiakiDebugLogging() const
{
    return verbosity == ChiakiLogVerbosity::Debug || verbosity == ChiakiLogVerbosity::Trace;
}

void DeveloperSettings::setCoreLog(log::CoreLog* log)
{
    coreLog = log;
    updateCoreLog();
}

void DeveloperSettings::followStream(bool streaming)
{
    streamActive = streaming;
    updateCoreLog();
}

void DeveloperSettings::updateCoreLog()
{
    if (coreLog)
        coreLog->follow(verbosity, streamActive, silenced);
}

std::string_view DeveloperSettings::targetKey(TakionTarget target)
{
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

ChiakiLogVerbosity DeveloperSettings::verbosityOf(const std::string& value)
{
    if (value == "info")
        return ChiakiLogVerbosity::Info;
    if (value == "debug")
        return ChiakiLogVerbosity::Debug;
    if (value == "trace")
        return ChiakiLogVerbosity::Trace;
    return ChiakiLogVerbosity::Normal;
}

const char* DeveloperSettings::textOf(ChiakiLogVerbosity value)
{
    switch (value) {
        case ChiakiLogVerbosity::Info:  return "info";
        case ChiakiLogVerbosity::Debug: return "debug";
        case ChiakiLogVerbosity::Trace: return "trace";
        case ChiakiLogVerbosity::Normal:
        default:                        return "normal";
    }
}

log::Level DeveloperSettings::appLevel(ChiakiLogVerbosity value)
{
    switch (value) {
        case ChiakiLogVerbosity::Debug: return log::Level::debug;
        case ChiakiLogVerbosity::Trace: return log::Level::verbose;
        case ChiakiLogVerbosity::Normal:
        case ChiakiLogVerbosity::Info:
        default:                        return log::Level::info;
    }
}

}
