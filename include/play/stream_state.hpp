#pragma once

#include "play/stream_profile.hpp"

namespace akira::cloud {
class CloudSettings;
}

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::play {

class StreamSettings;

class StreamState {
public:
    StreamState(const StreamSettings& stream, const cloud::CloudSettings& cloud, settings::DeveloperSettings& developer);

    StreamState(const StreamState&) = delete;
    StreamState& operator=(const StreamState&) = delete;

    bool isActive() const { return active; }
    void setActive(bool value);

    StreamProfile profile() const { return activeProfile; }
    void setProfile(StreamProfile value) { activeProfile = value; }
    bool cloudPscloud() const { return pscloud; }
    void setCloudPscloud(bool value) { pscloud = value; }

    bool easuEnabled() const;
    int easuTargetHeight() const;

private:
    const StreamSettings& stream;
    const cloud::CloudSettings& cloud;
    settings::DeveloperSettings& developer;

    bool active = false;
    StreamProfile activeProfile = StreamProfile::Local;
    bool pscloud = true;
};

}
