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

auto ConsoleShelf::onGrid(const ConsoleEntry& entry) -> bool
{
    if (entry.primaryAction() != ConsoleEntry::Action::Register)
        return true;
    return entry.facts().type == HostType::Manual;
}

auto ConsoleShelf::nearbyFor(const ConsoleEntry& entry) -> std::optional<Nearby>
{
    const ConsoleFacts& facts = entry.facts();
    if (onGrid(entry) || facts.type == HostType::Remote || !facts.discovered)
        return std::nullopt;
    return Nearby{ .catalogName = facts.catalogName,
        .name = entry.displayName(),
        .ps5 = facts.ps5,
        .standby = facts.standby,
        .address = facts.address };
}

auto ConsoleShelf::accountFor(const ConsoleEntry& entry) -> std::optional<AccountConsole>
{
    const ConsoleFacts& facts = entry.facts();
    if (onGrid(entry) || facts.type != HostType::Remote)
        return std::nullopt;
    return AccountConsole{ .catalogName = facts.catalogName,
        .name = entry.displayName(),
        .ps5 = facts.ps5,
        .remotePlay = !facts.remotePlayDisabled };
}

auto ConsoleShelf::card(const std::string& catalogName) const -> std::optional<Card>
{
    const std::vector<Card>& cards = state.get().cards;
    const auto found = std::ranges::find(cards, catalogName, &Card::catalogName);
    if (found == cards.end())
        return std::nullopt;
    return *found;
}

auto ConsoleShelf::modes() const -> std::vector<AddMode>
{
    std::vector<AddMode> offered{ AddMode::Automatic, AddMode::Manual };
    if (state.get().accountOffered)
        offered.push_back(AddMode::PlayStation);
    return offered;
}

auto ConsoleShelf::presenceOf(const Card& card) -> Presence
{
    switch (card.status)
    {
        case ConsoleEntry::Status::Ready: return Presence::Ready;
        case ConsoleEntry::Status::Standby: return Presence::RestMode;
        case ConsoleEntry::Status::Presence:
            return card.badge == ConsoleEntry::Badge::Remote || !card.action ? Presence::Unknown : Presence::NotNearby;
        case ConsoleEntry::Status::Link:
        case ConsoleEntry::Status::Register: break;
    }
    return Presence::Unknown;
}

void ConsoleShelf::show(const ConsoleList& list)
{
    std::vector<Card> cards;
    std::vector<Nearby> nearby;
    std::vector<AccountConsole> account;
    for (const ConsoleEntry& entry : list.shown())
    {
        if (onGrid(entry))
            cards.push_back(cardFor(entry));
        else if (std::optional<Nearby> found = nearbyFor(entry))
            nearby.push_back(std::move(*found));
        else if (std::optional<AccountConsole> listed = accountFor(entry))
            account.push_back(std::move(*listed));
    }
    state.update([&](View& view) {
        view.cards = std::move(cards);
        view.nearby = std::move(nearby);
        view.account = std::move(account);
        view.linkTargets = list.linkTargets();
        view.hasProfile = list.hasProfile();
    });
}

void ConsoleShelf::offerAccount(bool offered)
{
    state.update([offered](View& view) {
        view.accountOffered = offered;
        if (!offered && view.adding && view.adding->mode == AddMode::PlayStation)
            view.adding->mode = AddMode::Automatic;
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
    state.update([open](View& view) {
        if (!open)
            view.adding.reset();
        else if (!view.adding)
            view.adding = Adding{};
    });
}

void ConsoleShelf::chooseMode(AddMode mode)
{
    state.update([mode](View& view) {
        if (!view.adding || (mode == AddMode::PlayStation && !view.accountOffered))
            return;
        view.adding->mode = mode;
        view.adding->failure.reset();
    });
}

void ConsoleShelf::chooseAccountGeneration(bool ps5)
{
    state.update([ps5](View& view) {
        if (view.adding)
            view.adding->accountPs5 = ps5;
    });
}

void ConsoleShelf::lookup(Lookup phase)
{
    state.update([phase](View& view) {
        if (view.adding)
            view.adding->lookup = phase;
    });
}

void ConsoleShelf::addFailed(std::optional<std::string> failure)
{
    state.update([&failure](View& view) {
        if (view.adding)
            view.adding->failure = std::move(failure);
    });
}

void ConsoleShelf::scanned(Scan scan)
{
    state.write(&View::scan, std::move(scan));
}

void ConsoleShelf::edit(std::optional<Editing> editing)
{
    state.write(&View::editing, std::move(editing));
}

void ConsoleShelf::registering(std::optional<Registering> progress)
{
    state.write(&View::registering, std::move(progress));
}

}
