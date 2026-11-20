#pragma once

#include <arisa/composition/observable.hpp>

#include "play/rumble_profile.hpp"

namespace akira::play {
class ControllerSettings;
}

namespace akira::settings {
class SettingsStore;
}

namespace akira::settings::ui {

class ControlOptions {
public:
    enum class Swipe { Up, Down, Left, Right };

    struct Values {
        bool touchpad = true;
        bool swipeUp = true;
        bool swipeDown = true;
        bool swipeLeft = true;
        bool swipeRight = true;
        play::RumbleSource rumble = play::RumbleSource::Derived;
        play::HapticIntensity intensity = play::HapticIntensity::Normal;

        friend bool operator==(const Values&, const Values&) = default;
    };

    ControlOptions(play::ControllerSettings& controllers, SettingsStore& store);

    auto view() const -> const ::arisa::composition::Observable<Values>& { return state; }

    void setTouchpad(bool enabled);
    void setSwipe(Swipe direction, bool enabled);
    void setRumble(play::RumbleSource source);
    void setIntensity(play::HapticIntensity intensity);

private:
    void apply(Values values);

    play::ControllerSettings& controllers;
    SettingsStore& store;
    ::arisa::composition::Observable<Values> state;
};

}
