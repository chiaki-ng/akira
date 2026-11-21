#pragma once

#include <arisa/composition/observable.hpp>

#include "play/volume.hpp"

namespace akira::play {
class StreamSettings;
}

namespace akira::settings {
class SettingsStore;
}

namespace akira::settings::ui {

class SoundOptions {
public:
    struct Values {
        play::Volume volume;

        friend bool operator==(const Values&, const Values&) = default;
    };

    SoundOptions(play::StreamSettings& stream, SettingsStore& store);

    auto view() const -> const ::arisa::composition::Observable<Values>& { return state; }

    void setVolume(play::Volume volume);

private:
    play::StreamSettings& stream;
    SettingsStore& store;
    ::arisa::composition::Observable<Values> state;
};

}
