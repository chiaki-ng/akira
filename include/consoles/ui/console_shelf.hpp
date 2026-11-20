#pragma once

#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "consoles/console_entry.hpp"
#include "consoles/console_list.hpp"

namespace akira::consoles::ui {

class ConsoleShelf {
public:
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

        friend bool operator==(const Card&, const Card&) = default;
    };

    enum class Ask { RegisterHow, Pin, Delete, TakionVersion, ConsolePin, Address, Link, Notice };

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
        std::vector<std::string> linkTargets;
        bool hasProfile = false;
        bool adding = false;
        std::optional<Prompt> prompt;
        std::optional<Registering> registering;

        friend bool operator==(const View&, const View&) = default;
    };

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }
    auto view() -> ::arisa::composition::Observable<View>& { return state; }
    auto card(const std::string& catalogName) const -> std::optional<Card>;

    void show(const ConsoleList& list);
    void ask(Prompt prompt);
    void dismiss();
    void openAdd(bool open);
    void registering(std::optional<Registering> progress);

private:
    static Card cardFor(const ConsoleEntry& entry);

    ::arisa::composition::Observable<View> state;
};

}
