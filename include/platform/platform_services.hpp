#pragma once

class HttpPool;

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
}

namespace akira::network {
class WireGuardManager;
}

namespace akira::play {
class ControllerSettings;
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

namespace akira::platform
{

struct PlatformServices {
    play::StreamSettings& stream;
    play::StreamState& streamState;
    play::ControllerSettings& controllers;
    settings::DeveloperSettings& developer;
    app::AppSettings& app;
    accounts::AccountSettings& accounts;
    settings::SettingsStore& store;
    network::WireGuardManager& wireGuard;
    HttpPool& http;
    trophies::TrophyManager& trophies;
};

}
