#pragma once

#include <cstdint>
#include <optional>

#include <arisa/composition/observable.hpp>

#include "play/button_board.hpp"
#include "play/combo_capture.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::settings::ui {
class ControlOptions;
}

namespace akira::play {
class ButtonCapture;
class ControllerSettings;
}

namespace akira::play::ui {

class Mapping {
public:
    struct Status {
        int revision = 0;
        std::optional<std::uint32_t> capturing;
        std::uint64_t peak = 0;

        friend bool operator==(const Status&, const Status&) = default;
    };

    Mapping(ControllerSettings& controllers, settings::SettingsStore& store, settings::ui::ControlOptions& options,
        ButtonCapture& capture);
    ~Mapping();

    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;

    auto view() const -> const ::arisa::composition::Observable<Status>& { return state; }
    auto board() const -> const ButtonBoard& { return buttons; }
    auto options() const -> settings::ui::ControlOptions& { return touch; }

    void capture(std::uint32_t key);
    void poll();
    void cancel();
    void reset(std::uint32_t key);
    void resetAll();

private:
    void saved();

    ButtonBoard buttons;
    settings::SettingsStore& store;
    settings::ui::ControlOptions& touch;
    ButtonCapture& source;
    ComboCapture combo;
    ::arisa::composition::Observable<Status> state;
};

}
