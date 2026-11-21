#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <arisa/composition/observable.hpp>

#include "play/ui/connecting_screen.hpp"

namespace akira::app::arisa {

class ShellModel {
public:
    enum class Phase { Starting, Restricted, Home, Connecting, Streaming };
    enum class Page { Consoles, Cloud, Trophies };

    struct Ending {
        std::string title;
        std::string message;
        bool reconnect = false;
        bool retry = false;

        friend bool operator==(const Ending&, const Ending&) = default;
    };

    struct DnsWarning {
        std::string domain;
        std::string server;
        int wait = 0;

        friend bool operator==(const DnsWarning&, const DnsWarning&) = default;
    };

    struct View {
        Phase phase = Phase::Starting;
        play::ui::ConnectingScreen::Progress progress;
        std::optional<Ending> ending;
        bool settings = false;
        Page page = Page::Consoles;
        bool cloudOffered = false;
        bool trophiesOffered = false;
        bool exiting = false;
        bool exitConfirmed = false;
        std::optional<DnsWarning> dns;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }

    void ready();
    void restricted();
    void connecting(std::string console);
    void stage(play::ui::ConnectingScreen::Progress progress);
    void streaming();
    void home();
    void ended(Ending ending);
    void dismissEnding();
    void showSettings(bool shown);
    void show(Page page);
    void offer(bool cloud, bool trophies);
    void askExit();
    void dismissExit();
    void confirmExit();
    void warnDns(std::string domain, std::string server, std::chrono::steady_clock::time_point now);
    void tick(std::chrono::steady_clock::time_point now);
    void dismissDns();

private:
    ::arisa::composition::Observable<View> state;
    std::optional<std::chrono::steady_clock::time_point> dnsShown;
};

}
