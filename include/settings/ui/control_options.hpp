#pragma once

#include <arisa/composition/observable.hpp>

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

        friend bool operator==(const Values&, const Values&) = default;
    };

    ControlOptions(play::ControllerSettings& controllers, SettingsStore& store);

    auto view() const -> const ::arisa::composition::Observable<Values>& { return state; }

    void setTouchpad(bool enabled);
    void setSwipe(Swipe direction, bool enabled);
    void enableAll();

private:
    void apply(Values values);

    play::ControllerSettings& controllers;
    SettingsStore& store;
    ::arisa::composition::Observable<Values> state;
};

}
