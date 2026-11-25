#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "consoles/console_entry.hpp"
#include "consoles/console_list.hpp"

namespace akira::consoles::ui {

class ConsoleShelf {
public:
    enum class Presence { Unknown, Ready, RestMode, NotNearby };

    struct Card {
        std::string catalogName;
        std::string name;
        bool ps5 = false;
        ConsoleEntry::Badge badge = ConsoleEntry::Badge::Local;
        ConsoleEntry::Status status = ConsoleEntry::Status::Presence;
        std::string presence;
        bool dimmed = false;
        std::optional<ConsoleEntry::Action> action;
        std::vector<ConsoleEntry::Option> options;
        std::string target;
        ConsoleEntry::Route route = ConsoleEntry::Route::Home;
        std::vector<ConsoleEntry::Route> routes;

        friend bool operator==(const Card&, const Card&) = default;
    };

    struct Nearby {
        std::string catalogName;
        std::string name;
        bool ps5 = false;
        bool standby = false;
        std::string address;

        friend bool operator==(const Nearby&, const Nearby&) = default;
    };

    struct AccountConsole {
        std::string catalogName;
        std::string name;
        bool ps5 = false;
        bool remotePlay = true;

        friend bool operator==(const AccountConsole&, const AccountConsole&) = default;
    };

    enum class AddMode { Automatic, Manual, PlayStation };
    enum class Lookup { Idle, Loading, Ready, Failed };

    struct Adding {
        AddMode mode = AddMode::Automatic;
        Lookup lookup = Lookup::Idle;
        bool accountPs5 = true;
        std::optional<std::string> failure;

        friend bool operator==(const Adding&, const Adding&) = default;
    };

    struct Scan {
        bool running = false;
        bool searched = false;
        std::string broadcast;
        std::vector<std::string> unicast;
        std::string address;

        friend bool operator==(const Scan&, const Scan&) = default;
    };

    struct Edit {
        std::string address;
        std::string pin;
        int takion = 0;

        friend bool operator==(const Edit&, const Edit&) = default;
    };

    struct Editing {
        std::string catalogName;
        std::string name;
        bool ps5 = false;
        Edit values;
        bool remote = false;
        bool address = false;
        bool pin = false;
        std::vector<std::string> takion;
        bool deletable = false;

        friend bool operator==(const Editing&, const Editing&) = default;
    };

    enum class Ask { RegisterHow, Pin, Delete, Link, Notice };

    struct Prompt {
        Ask ask = Ask::Notice;
        std::string catalogName;
        std::string text;
        std::vector<std::string> choices;
        int selected = 0;

        friend bool operator==(const Prompt&, const Prompt&) = default;
    };

    struct Registering {
        std::string catalogName;
        std::string stage;

        friend bool operator==(const Registering&, const Registering&) = default;
    };

    struct View {
        std::vector<Card> cards;
        std::vector<Nearby> nearby;
        std::vector<AccountConsole> account;
        std::vector<std::string> linkTargets;
        bool hasProfile = false;
        bool accountOffered = false;
        std::optional<Adding> adding;
        Scan scan;
        std::optional<Editing> editing;
        std::optional<Prompt> prompt;
        std::optional<Registering> registering;
        std::map<std::string, ConsoleEntry::Route> chosen;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }
    auto view() -> ::arisa::composition::Observable<View>& { return state; }
    auto card(const std::string& catalogName) const -> std::optional<Card>;
    auto modes() const -> std::vector<AddMode>;

    void show(const ConsoleList& list);
    void offerAccount(bool offered);
    void ask(Prompt prompt);
    void dismiss();
    void openAdd(bool open);
    void chooseMode(AddMode mode);
    void chooseRoute(const std::string& catalogName, ConsoleEntry::Route route);
    void chooseAccountGeneration(bool ps5);
    void lookup(Lookup phase);
    void addFailed(std::optional<std::string> failure);
    void scanned(Scan scan);
    void edit(std::optional<Editing> editing);
    void registering(std::optional<Registering> progress);

    static auto presenceOf(const Card& card) -> Presence;

private:
    static auto cardFor(const ConsoleEntry& entry, std::optional<ConsoleEntry::Route> chosen) -> Card;
    static auto onGrid(const ConsoleEntry& entry) -> bool;
    static auto nearbyFor(const ConsoleEntry& entry) -> std::optional<Nearby>;
    static auto accountFor(const ConsoleEntry& entry) -> std::optional<AccountConsole>;

    ::arisa::composition::Observable<View> state;
};

}
