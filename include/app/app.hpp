#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <switch.h>

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/services.hpp"
#include "app/view_services.hpp"
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

namespace akira::app {

class App {
public:
    App(int argc, char* argv[]);

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    static constexpr const char* SettingsDirectory = "sdmc:/switch/akira";

    static Result prepareProcess();
    static const char* appletTypeName(AppletType type);
    static bool fsr4k();

    void openLog();
    void readArguments();
    void prepareLocale();
    void startServices();
    [[nodiscard]] bool startFrameworks();
    std::function<void()> signInRenewed();
    void startCore();
    void startAsyncLogging();
    void registerViews();
    void openWindow();
    void runMainLoop();
    void shutDown();

    int argc;
    char** argv;
    Result mainPlaced;
    network::CustomDns dns;
    network::ResolverHook resolverHook;
    settings::SettingsStore store;
    settings::DeveloperSettings developer;
    play::StreamSettings stream;
    play::ControllerSettings controllers;
    cloud::CloudSettings cloudSettings;
    network::NetworkSettings networkSettings;
    accounts::AccountSettings accountSettings;
    consoles::ConsoleSettings consoleSettings;
    AppSettings appSettings;
    consoles::HostCatalog hosts;
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
    std::optional<Services> services;
    std::optional<ViewServices> viewServices;
    std::optional<play::ChiakiCore> core;
};

}
