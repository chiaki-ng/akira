#pragma once

#include <optional>
#include <string>

#include <arisa/composition/observable.hpp>

#include "play/ui/connecting_screen.hpp"

namespace akira::app::arisa {

class ShellModel {
public:
    enum class Phase { Starting, Home, Connecting, Streaming };
    enum class Page { Consoles, Cloud, Trophies };

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
        bool settings = false;
        Page page = Page::Consoles;
        bool cloudOffered = false;
        bool trophiesOffered = false;

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
    void showSettings(bool shown);
    void show(Page page);
    void offer(bool cloud, bool trophies);

private:
    ::arisa::composition::Observable<View> state;
};

}
