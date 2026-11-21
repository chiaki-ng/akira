#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/services.hpp"
#include "app/ui_dispatcher.hpp"
#include "app/ui_window.hpp"
#include "cloud/cloud_settings.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "consoles/console_settings.hpp"
#include "consoles/discovery_gate.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/host_catalog.hpp"
#include "consoles/register_flow.hpp"
#include "log/ffmpeg_log.hpp"
#include "network/custom_dns.hpp"
#include "network/network_settings.hpp"
#include "network/resolver_hook.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/chiaki_core.hpp"
#include "play/controller_settings.hpp"
#include "play/lazy_session.hpp"
#include "play/stream_preferences.hpp"
#include "play/stream_settings.hpp"
#include "play/stream_state.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
#include "trophies/trophy_manager.hpp"
#include "util/http_pool.hpp"

namespace akira::platform {
class PadRumble;
}

namespace akira::play {
class AudioOutput;
}

namespace akira::platform::video {
class GpuAccess;
class OverlayPalette;
}

namespace akira::app {

struct UiSeams {
    UiDispatcher& dispatcher;
    UiWindow& window;
    const Strings& strings;
    platform::video::GpuAccess& gpu;
    platform::PadRumble& rumble;
    play::AudioOutput& audio;
    const platform::video::OverlayPalette& overlayPalette;
};

class ServiceGraph {
public:
    explicit ServiceGraph(UiSeams seams);
    ~ServiceGraph();

    ServiceGraph(const ServiceGraph&) = delete;
    ServiceGraph& operator=(const ServiceGraph&) = delete;

    static void prepareProcess();

    void openLog();
    void startFfmpegLog();
    [[nodiscard]] bool startCurl();
    void startCore();
    void startServices();
    [[nodiscard]] bool startSdl(bool audio);
    void followFocus();
    void startAsyncLogging();
    void shutDown();

    auto services() -> Services& { return *services_; }
    auto platform() -> platform::PlatformFactory& { return *factory; }
    auto appSettings() -> AppSettings& { return appSettings_; }
    auto developer() -> settings::DeveloperSettings& { return developer_; }

private:
    static constexpr const char* SettingsDirectory = "sdmc:/switch/akira";

    static bool fsr4k();
    std::function<void()> signInRenewed();
    void flushLog();

    UiSeams seams;
    network::CustomDns dns;
    network::ResolverHook resolverHook;
    settings::SettingsStore store;
    settings::DeveloperSettings developer_;
    play::StreamSettings stream;
    play::ControllerSettings controllers;
    cloud::CloudSettings cloudSettings;
    network::NetworkSettings networkSettings;
    accounts::AccountSettings accountSettings;
    consoles::ConsoleSettings consoleSettings;
    AppSettings appSettings_;
    std::optional<play::ChiakiCore> core;
    std::optional<consoles::HostCatalog> hosts;
    consoles::DiscoveryGate discoveryGate;
    consoles::RegisterFlow registration;
    play::StreamState streamState;
    std::optional<log::FfmpegLog> ffmpegLog;
    std::optional<HttpPool> http;
    std::optional<network::WireGuardManager> wireGuard;
    std::optional<trophies::TrophyManager> trophies;
#if AKIRA_CLOUD
    std::optional<cloud::Service> cloud;
#endif
    std::unique_ptr<platform::PlatformFactory> factory;
    std::optional<consoles::DiscoveryManager> discovery;
    std::optional<play::StreamPreferences> preferences;
    std::optional<play::LazySession> session;
    std::optional<Services> services_;
    std::vector<std::unique_ptr<UiWindow::FocusWatch>> focusWatches;
    std::unique_ptr<UiDispatcher::Repeating> logFlush;
    std::shared_ptr<std::atomic<bool>> flushInFlight = std::make_shared<std::atomic<bool>>(false);
};

}
