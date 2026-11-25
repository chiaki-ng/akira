#pragma once

namespace libchiaking::platform {
class ThreadingProvider;
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
}

namespace akira::platform {
class PadRumble;
}

namespace akira::platform::video {
class GpuAccess;
class OverlayPalette;
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
    libchiaking::platform::ThreadingProvider& threading;
    video::GpuAccess& gpu;
    PadRumble& rumble;
    const video::OverlayPalette& overlayPalette;
};

}
