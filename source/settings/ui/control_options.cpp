#include "settings/ui/control_options.hpp"

#include "play/controller_settings.hpp"
#include "settings/settings_store.hpp"

#include <utility>

namespace akira::settings::ui {

ControlOptions::ControlOptions(play::ControllerSettings& controllers, SettingsStore& store)
    : controllers(controllers)
    , store(store)
    , state(Values{
          .touchpad = controllers.touchpadEnabled(),
          .swipeUp = controllers.swipeUpEnabled(),
          .swipeDown = controllers.swipeDownEnabled(),
          .swipeLeft = controllers.swipeLeftEnabled(),
          .swipeRight = controllers.swipeRightEnabled(),
      })
{
}

void ControlOptions::setTouchpad(bool enabled)
{
    Values values = state.get();
    values.touchpad = enabled;
    apply(values);
}

void ControlOptions::setSwipe(Swipe direction, bool enabled)
{
    Values values = state.get();
    switch (direction)
    {
        case Swipe::Up: values.swipeUp = enabled; break;
        case Swipe::Down: values.swipeDown = enabled; break;
        case Swipe::Left: values.swipeLeft = enabled; break;
        case Swipe::Right: values.swipeRight = enabled; break;
    }
    apply(values);
}

void ControlOptions::enableAll()
{
    apply(Values{});
}

void ControlOptions::apply(Values values)
{
    if (values == state.get())
        return;
    controllers.setTouchpadEnabled(values.touchpad);
    controllers.setSwipeUpEnabled(values.swipeUp);
    controllers.setSwipeDownEnabled(values.swipeDown);
    controllers.setSwipeLeftEnabled(values.swipeLeft);
    controllers.setSwipeRightEnabled(values.swipeRight);
    state.set(std::move(values));
    store.save();
}

}
