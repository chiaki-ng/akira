#pragma once

#include <arisa/composition/observable.hpp>

#include "play/colour_adjustments.hpp"
#include "play/picture_profile.hpp"

namespace akira::play {
class StreamSettings;
}

namespace akira::settings {
class SettingsStore;
}

namespace akira::settings::ui {

class PictureOptions {
public:
    struct Values {
        play::PictureProfile local;
        play::PictureProfile remote;
        play::PictureProfile vpn;
        bool rcas = false;
        int rcasStrength = 0;
        play::ColourAdjustments colour;

        friend bool operator==(const Values&, const Values&) = default;
    };

    PictureOptions(play::StreamSettings& stream, SettingsStore& store, bool fsr4k);

    auto view() const -> const ::arisa::composition::Observable<Values>& { return state; }
    auto profile(play::StreamProfile profile) const -> const play::PictureProfile&;

    void chooseResolution(play::StreamProfile profile, play::VideoResolution resolution);
    void chooseUpscale(play::StreamProfile profile, int target);
    void chooseFps(play::StreamProfile profile, play::VideoFps fps);
    void chooseBitrate(play::StreamProfile profile, int bitrate);
    void setRcas(bool enabled);
    void setRcasStrength(int percent);
    void setColour(play::ColourAdjustments colour);

private:
    template <class Change>
    void change(play::StreamProfile profile, Change&& edit);
    void save();

    play::StreamSettings& stream;
    SettingsStore& store;
    ::arisa::composition::Observable<Values> state;
};

}
