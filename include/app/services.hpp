#pragma once

class HttpPool;

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
class Strings;
class UiDispatcher;
class UiWindow;
}

namespace akira::cloud {
class CloudSettings;
class Service;
}

namespace akira::consoles {
class ConsoleSettings;
class DiscoveryGate;
class DiscoveryManager;
class HostCatalog;
class RegisterFlow;
}

namespace akira::network {
class CustomDns;
class NetworkSettings;
class WireGuardManager;
}

namespace akira::platform {
class PlatformFactory;
}

namespace akira::play {
class ControllerSettings;
class LazySession;
class StreamPreferences;
class StreamSettings;
class StreamState;
}

namespace akira::settings {
class DeveloperSettings;
class SettingsStore;
}

namespace akira::trophies {
class TrophyManager;
}

namespace akira::app {

struct Services {
    UiDispatcher& dispatcher;
    UiWindow& window;
    const Strings& strings;
    settings::SettingsStore& store;
    settings::DeveloperSettings& developer;
    play::StreamSettings& stream;
    play::ControllerSettings& controllers;
    cloud::CloudSettings& cloudSettings;
    network::NetworkSettings& networkSettings;
    accounts::AccountSettings& accounts;
    consoles::ConsoleSettings& consoleSettings;
    AppSettings& appSettings;
    consoles::HostCatalog& hosts;
    play::StreamState& streamState;
    network::CustomDns& dns;
    HttpPool& http;
    platform::PlatformFactory& platform;
    network::WireGuardManager& wireGuard;
    consoles::DiscoveryManager& discovery;
    consoles::DiscoveryGate& discoveryGate;
    consoles::RegisterFlow& registration;
    play::StreamPreferences& preferences;
    play::LazySession& session;
    trophies::TrophyManager& trophies;
#if AKIRA_CLOUD
    cloud::Service& cloud;
#endif
};

}
