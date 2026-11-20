#include "consoles/ui/console_shelf.hpp"

#include <algorithm>
#include <utility>

namespace akira::consoles::ui {

auto ConsoleShelf::cardFor(const ConsoleEntry& entry) -> Card
{
    return Card{
        .catalogName = entry.facts().catalogName,
        .name = entry.displayName(),
        .ps5 = entry.facts().ps5,
        .badge = entry.badge(),
        .status = entry.status(),
        .presence = entry.facts().presence,
        .dimmed = entry.dimmed(),
        .action = entry.primaryAction(),
        .options = entry.options(),
    };
}

auto ConsoleShelf::card(const std::string& catalogName) const -> std::optional<Card>
{
    const std::vector<Card>& cards = state.get().cards;
    const auto found = std::ranges::find(cards, catalogName, &Card::catalogName);
    if (found == cards.end())
        return std::nullopt;
    return *found;
}

void ConsoleShelf::show(const ConsoleList& list)
{
    std::vector<Card> cards;
    for (const ConsoleEntry& entry : list.shown())
        cards.push_back(cardFor(entry));
    state.update([&](View& view) {
        view.cards = std::move(cards);
        view.linkTargets = list.linkTargets();
        view.hasProfile = list.hasProfile();
    });
}

void ConsoleShelf::ask(Prompt prompt)
{
    state.write(&View::prompt, std::optional<Prompt>(std::move(prompt)));
}

void ConsoleShelf::dismiss()
{
    state.write(&View::prompt, std::optional<Prompt>());
}

void ConsoleShelf::openAdd(bool open)
{
    state.write(&View::adding, open);
}

void ConsoleShelf::registering(std::optional<Registering> progress)
{
    state.write(&View::registering, std::move(progress));
}

}
