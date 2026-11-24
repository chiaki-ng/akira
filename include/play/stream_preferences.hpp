#pragma once

#include <cstdint>
#include <optional>

#include <libchiaking/play.hpp>

namespace akira::cloud {
class CloudLaunch;
class CloudSettings;
}

namespace akira::consoles {
class Host;
}

namespace akira::network {
class NetworkSettings;
class WireGuardManager;
}

namespace akira::platform {
class PlatformFactory;
}

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::play {

class StreamSettings;

class StreamPreferences {
public:
    StreamPreferences(const StreamSettings& stream, const cloud::CloudSettings& cloud,
                      const network::NetworkSettings& network, const settings::DeveloperSettings& developer,
                      const platform::PlatformFactory& platform, network::WireGuardManager& wireGuard);

    auto forConsole(consoles::Host& host) const -> std::optional<libchiaking::play::Preferences>;
    auto forCloud(const cloud::CloudLaunch& launch) const -> std::optional<libchiaking::play::Preferences>;

private:
    auto tuned(libchiaking::play::PictureRequest picture, std::uint32_t takionVersion, bool negotiate) const
        -> std::optional<libchiaking::play::Preferences>;

    const StreamSettings& streamSettings;
    const cloud::CloudSettings& cloudSettings;
    const network::NetworkSettings& networkSettings;
    const settings::DeveloperSettings& developerSettings;
    const platform::PlatformFactory& platformFactory;
    network::WireGuardManager& wireGuard;
};

}
