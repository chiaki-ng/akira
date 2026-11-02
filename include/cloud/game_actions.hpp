#ifndef AKIRA_CLOUD_GAME_ACTIONS_HPP
#define AKIRA_CLOUD_GAME_ACTIONS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cloud/models.hpp"

namespace cloud {

class CardCheck {
public:
    enum class Kind {
        Checking,
        Streams,
        NotStreamable,
        Refused,
        Unavailable
    };

    static auto checking() -> CardCheck { return CardCheck{Kind::Checking, {}}; }
    static auto streams() -> CardCheck { return CardCheck{Kind::Streams, {}}; }
    static auto notStreamable() -> CardCheck { return CardCheck{Kind::NotStreamable, {}}; }
    static auto refused() -> CardCheck { return CardCheck{Kind::Refused, {}}; }
    static auto unavailable(std::string detail) -> CardCheck { return CardCheck{Kind::Unavailable, std::move(detail)}; }

    auto kind() const noexcept -> Kind { return checkKind; }
    auto detail() const noexcept -> const std::string& { return unavailableDetail; }
    bool allowsPlay() const noexcept { return checkKind == Kind::Streams; }
    bool refusesStreaming() const noexcept { return checkKind == Kind::NotStreamable || checkKind == Kind::Refused; }
    bool isSettled() const noexcept { return checkKind != Kind::Checking && checkKind != Kind::Unavailable; }

    static bool needed(const Game& game) { return game.needsStorefrontCheck; }

    friend bool operator==(const CardCheck&, const CardCheck&) = default;

private:
    CardCheck(Kind kind, std::string detail) : checkKind(kind), unavailableDetail(std::move(detail)) {}

    Kind checkKind;
    std::string unavailableDetail;
};

class GameActions {
public:
    enum class Offer {
        CheckAndPlay,
        ChooseGame
    };

    enum class StoreTitle {
        ViewInStore,
        ViewStorePage
    };

    enum class Focus {
        Play,
        Store,
        AddToLibrary,
        RefreshTitle
    };

    struct Play {
        std::optional<Offer> offer;
        bool allowed = false;

        bool enabled() const { return offer.has_value() && allowed; }
    };

    GameActions(const Game& game, bool signedIn, const std::optional<CardCheck>& check, bool titleRefreshIdle);

    auto play() const -> const std::optional<Play>& { return playButton; }
    auto store() const -> const std::optional<StoreTitle>& { return storeLink; }
    bool addsToLibrary() const { return addToLibrary; }
    bool refreshTitleFocusable() const { return refreshFocusable; }
    bool showsLaunchChecks() const { return launchChecks; }
    bool showsLibraryAddHint() const { return addToLibrary; }
    bool showsTitleNotice() const { return launchChecks || addToLibrary; }
    auto focusOrder() const -> std::vector<Focus>;

    static auto standingAfter(const Game& game, const std::optional<CardCheck>& check) -> Standing;
    static bool isStoreUrl(std::string_view url);

private:
    static auto offerFor(const Game& game, bool signedIn) -> std::optional<Offer>;

    std::optional<Play> playButton;
    std::optional<StoreTitle> storeLink;
    bool addToLibrary = false;
    bool refreshFocusable = false;
    bool launchChecks = false;
};

} // namespace cloud

#endif // AKIRA_CLOUD_GAME_ACTIONS_HPP
