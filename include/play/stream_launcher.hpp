#pragma once

#include <expected>
#include <memory>
#include <string>

#include <libchiaking/play.hpp>

#include "play/connection.hpp"

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class UiDispatcher;
}

namespace akira::consoles {
class Host;
}

namespace akira::platform {
class PlatformFactory;
}

namespace akira::play {

class ChiakiCore;
class Session;
class StreamPreferences;

class StreamLauncher {
public:
    enum class Failure { NotRegistered, NoPreferences, Codec, Video, Worker };

    StreamLauncher(Session& session, platform::PlatformFactory& platform, const StreamPreferences& preferences,
        const accounts::AccountSettings& accounts, app::UiDispatcher& dispatcher, ChiakiCore& core);

    [[nodiscard]] auto open(consoles::Host& host, std::shared_ptr<consoles::Host> owner,
        std::weak_ptr<ConnectionListener> listener, libchiaking::play::ConsoleHandover handover,
        const std::string& context) -> std::expected<std::shared_ptr<Connection>, Failure>;

private:
    struct Picture {
        int width = 0;
        int height = 0;
        int fps = 60;
        int bitrate = 0;
        bool hevc = false;
    };

    static auto pictureOf(const libchiaking::play::Picture& picture) -> Picture;
    void applyPadProfile();

    Session& session;
    platform::PlatformFactory& platform;
    const StreamPreferences& preferences;
    const accounts::AccountSettings& accounts;
    app::UiDispatcher& dispatcher;
    ChiakiCore& core;
};

}
