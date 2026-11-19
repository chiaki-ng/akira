#pragma once

#include <memory>

namespace libchiaking::platform {
class ThreadingProvider;
}

namespace akira::platform {
class PlatformFactory;
}

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::app {
class UiWindow;
}

namespace akira::play {

class ControllerSettings;
class Session;
class StreamState;

class LazySession {
public:
    LazySession(platform::PlatformFactory& platform, StreamState& streamState, const ControllerSettings& controllers,
                const settings::DeveloperSettings& developer, app::UiWindow& window,
                libchiaking::platform::ThreadingProvider& threading);
    ~LazySession();

    LazySession(const LazySession&) = delete;
    LazySession& operator=(const LazySession&) = delete;

    Session& get();
    void abandon();

private:
    platform::PlatformFactory& platform;
    StreamState& streamState;
    const ControllerSettings& controllers;
    const settings::DeveloperSettings& developer;
    app::UiWindow& window;
    libchiaking::platform::ThreadingProvider& threading;
    std::unique_ptr<Session> session;
};

}
