#include "cloud/game_actions.hpp"

namespace cloud {

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

} // namespace cloud
