#pragma once

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
}

namespace akira::platform
{

struct PlatformServices {
    play::StreamSettings& stream;
    play::StreamState& streamState;
    play::ControllerSettings& controllers;
    settings::DeveloperSettings& developer;
    app::AppSettings& app;
    network::WireGuardManager& wireGuard;
};

}
