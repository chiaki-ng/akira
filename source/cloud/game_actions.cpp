#include "cloud/game_actions.hpp"

#include <algorithm>
#include <cctype>

namespace akira::cloud {

GameActions::GameActions(const Game& game, bool signedIn, const std::optional<CardCheck>& check,
                         bool titleRefreshIdle)
{
    const bool storeUrl = isStoreUrl(game.conceptUrl);
    if (game.trialEnded)
    {
        storeLink = StoreTitle::ViewInStore;
        return;
    }

    const bool inLibrary = !game.needsLibraryAdd;
    if (inLibrary)
        playButton = Play{offerFor(game, signedIn), !CardCheck::needed(game) || (check && check->allowsPlay())};
    launchChecks = inLibrary;

    const Standing standing = standingAfter(game, check);
    const bool refusedByCheck = check && check->refusesStreaming();
    if (inLibrary && CardCheck::needed(game) && (standing == Standing::MayStream || refusedByCheck))
        storeLink = standing == Standing::Store ? StoreTitle::ViewInStore : StoreTitle::ViewStorePage;
    else if (storeUrl && (game.isStoreOnly() || !game.canBeAddedToLibrary))
        storeLink = StoreTitle::ViewInStore;
    else
        addToLibrary = storeUrl;

    refreshFocusable = addToLibrary && titleRefreshIdle;
}

auto GameActions::focusOrder() const -> std::vector<Focus>
{
    std::vector<Focus> order;
    if (playButton && playButton->enabled())
        order.push_back(Focus::Play);
    if (storeLink)
        order.push_back(Focus::Store);
    else if (addToLibrary)
    {
        order.push_back(Focus::AddToLibrary);
        if (refreshFocusable)
            order.push_back(Focus::RefreshTitle);
    }
    return order;
}

auto GameActions::refocus(std::optional<Focus> previous, bool playWasEnabled,
                          const std::function<bool(Focus)>& focusable) const -> std::optional<Focus>
{
    const auto available = [&](Focus focus) -> std::optional<Focus> {
        return focusable(focus) ? std::optional(focus) : std::nullopt;
    };
    const bool playEnabled = focusable(Focus::Play);
    std::optional<Focus> target;
    if (playEnabled && !playWasEnabled && (!previous || previous == Focus::Play))
        target = available(Focus::Play);
    else if (previous)
        target = available(*previous);
    if (!target && previous == Focus::AddToLibrary)
        target = available(Focus::RefreshTitle);
    if (!target)
        for (Focus focus : focusOrder())
            if ((target = available(focus)))
                break;
    return target;
}

auto GameActions::standingNote(Standing standing, const Game& game) -> StandingNote
{
    if (game.isTrial && standing != Standing::Store && standing != Standing::OwnershipUnknown)
        return namesDemo(game.name) ? StandingNote::Demo : StandingNote::Trial;
    switch (standing)
    {
        case Standing::MayStream: return StandingNote::MayStream;
        case Standing::InLibrary: return StandingNote::InLibrary;
        case Standing::Included: return StandingNote::Included;
        case Standing::Store:
        case Standing::OwnershipUnknown: break;
    }
    return StandingNote::Store;
}

bool GameActions::namesDemo(const std::string& name)
{
    std::string lower = name;
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower == "demo" || lower.starts_with("demo ") || lower.ends_with(" demo") ||
           lower.find(" demo ") != std::string::npos;
}

auto GameActions::bundleChoices(const Game& game, const std::vector<Game>& listed) -> std::vector<Game>
{
    std::vector<Game> choices;
    for (const BundledGame& child : game.bundle)
    {
        if (!child.launchable || child.productId.empty())
            continue;
        auto match = std::ranges::find_if(listed, [&](const Game& candidate) {
            return candidate.productId != game.productId &&
                   (candidate.productId == child.productId ||
                    (!child.entitlementId.empty() && candidate.entitlementId == child.entitlementId));
        });
        Game choice = match != listed.end() ? *match : game;
        choice.productId = child.productId;
        choice.bundle.clear();
        choice.launchBlock = LaunchBlock::None;
        choices.push_back(std::move(choice));
    }
    return choices;
}

auto GameActions::standingAfter(const Game& game, const std::optional<CardCheck>& check) -> Standing
{
    if (check && check->refusesStreaming())
        return Standing::Store;
    if (game.standing != Standing::MayStream || !check || check->kind() != CardCheck::Kind::Streams)
        return game.standing;
    if (game.isOwnedPS5())
        return Standing::InLibrary;
    if (game.isInCatalog())
        return Standing::Included;
    return game.isOwned ? Standing::InLibrary : Standing::Store;
}

bool GameActions::isStoreUrl(std::string_view url)
{
    return url.starts_with("https://store.playstation.com/");
}

auto GameActions::offerFor(const Game& game, bool signedIn) -> std::optional<Offer>
{
    if (!signedIn)
        return std::nullopt;
    if (game.opensBundlePicker())
        return Offer::ChooseGame;
    if (game.launchBlock != LaunchBlock::None || !game.launchable())
        return std::nullopt;
    return Offer::CheckAndPlay;
}

} // namespace akira::cloud
