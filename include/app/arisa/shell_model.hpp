#pragma once

#include <optional>
#include <string>

#include <arisa/composition/observable.hpp>

#include "play/ui/connecting_screen.hpp"

namespace akira::app::arisa {

class ShellModel {
public:
    enum class Phase { Starting, Home, Connecting, Streaming };

    struct Ending {
        std::string title;
        std::string message;
        bool reconnect = false;

        friend bool operator==(const Ending&, const Ending&) = default;
    };

    struct View {
        Phase phase = Phase::Starting;
        play::ui::ConnectingScreen::Progress progress;
        std::optional<Ending> ending;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void ready();
    void connecting(std::string console);
    void stage(play::ui::ConnectingScreen::Progress progress);
    void streaming();
    void home();
    void ended(Ending ending);
    void dismissEnding();

private:
    ::arisa::composition::Observable<View> state;
};

}
