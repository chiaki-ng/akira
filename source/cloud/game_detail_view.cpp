#include "cloud/game_detail_view.hpp"

#include "cloud/cloud_connect_task.hpp"
#include "ui/qr_code_view.hpp"
#include "ui/theme.hpp"
#include "views/connecting_view.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>

using namespace brls::literals;

namespace cloud {

GameDetailView::GameDetailView(Game shownGame, std::vector<Game> listedGames)
    : game(std::move(shownGame)), listed(std::move(listedGames))
{
    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
    this->setBackgroundColor(akira::ui::active().backgroundDeep);
    this->setFocusable(true);
    this->setHideHighlight(true);

    if (game.refused)
        cardCheck = CardCheck::refused();

    buildStage();
    buildBand();
    render();

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    loadDetails();
    startCardCheck();
}

GameDetailView::~GameDetailView()
{
    *alive = false;
    static_cast<void>(lifetime.request_stop());
    static_cast<void>(launchStop.request_stop());
}

auto GameDetailView::checkOf(libchiaking::catalog::LaunchCheck check) -> LaunchChecks::Check
{
    using Core = libchiaking::catalog::LaunchCheck;
    switch (check)
    {
        case Core::loading_library: return LaunchChecks::Check::LoadingLibrary;
        case Core::membership: return LaunchChecks::Check::Membership;
        case Core::cloud_streaming: return LaunchChecks::Check::CloudStreaming;
        case Core::ownership: return LaunchChecks::Check::Ownership;
        case Core::entitlement: return LaunchChecks::Check::Entitlement;
        case Core::cloud_game: return LaunchChecks::Check::CloudGame;
        case Core::exclusions: return LaunchChecks::Check::Exclusions;
        case Core::release: return LaunchChecks::Check::Release;
    }
    return LaunchChecks::Check::LoadingLibrary;
}

auto GameDetailView::nowMs() -> int64_t
{
    const auto system = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return Service::instance().trustedNowMs().value_or(system);
}

void GameDetailView::buildStage()
{
    const auto& pal = akira::ui::active();

    stage = new brls::Box();
    stage->setWidthPercentage(100.0f);
    stage->setHeight(kStageHeight);
    stage->setAxis(brls::Axis::COLUMN);
    stage->setJustifyContent(brls::JustifyContent::FLEX_END);
    this->addView(stage);

    backdrop = new brls::Image();
    backdrop->setPositionType(brls::PositionType::ABSOLUTE);
    backdrop->setPositionTop(0);
    backdrop->setPositionLeft(0);
    backdrop->setWidthPercentage(100.0f);
    backdrop->setHeight(kStageHeight);
    backdrop->setScalingType(brls::ImageScalingType::FILL);
    stage->addView(backdrop);

    hero = new brls::Image();
    hero->setPositionType(brls::PositionType::ABSOLUTE);
    hero->setPositionTop(0);
    hero->setPositionLeft(0);
    hero->setWidthPercentage(100.0f);
    hero->setHeight(kStageHeight);
    hero->setScalingType(brls::ImageScalingType::FIT);
    hero->setImageAlign(brls::ImageAlignment::CENTER);
    hero->setVisibility(brls::Visibility::GONE);
    stage->addView(hero);

    auto* shade = new StageShade();
    shade->setPositionType(brls::PositionType::ABSOLUTE);
    shade->setPositionTop(0);
    shade->setPositionLeft(0);
    shade->setWidthPercentage(100.0f);
    shade->setHeight(kStageHeight);
    stage->addView(shade);

    auto* column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setWidth(600);
    column->setMarginLeft(kInset);
    column->setMarginBottom(24);
    stage->addView(column);

    logo = new brls::Image();
    logo->setWidth(430);
    logo->setHeight(150);
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setImageAlign(brls::ImageAlignment::LEFT);
    logo->setMarginBottom(20);
    logo->setVisibility(brls::Visibility::GONE);
    column->addView(logo);

    titleLabel = new brls::Label();
    titleLabel->setText(game.name);
    titleLabel->setFontSize(32);
    titleLabel->setTextColor(pal.text);
    titleLabel->setWidth(470);
    titleLabel->setMarginBottom(20);
    column->addView(titleLabel);

    metadataRow = new brls::Box();
    metadataRow->setAxis(brls::Axis::ROW);
    metadataRow->setAlignItems(brls::AlignItems::CENTER);
    metadataRow->setMarginBottom(18);
    column->addView(metadataRow);

    actionsRow = new brls::Box();
    actionsRow->setAxis(brls::Axis::ROW);
    actionsRow->setAlignItems(brls::AlignItems::CENTER);
    actionsRow->setMarginBottom(12);
    column->addView(actionsRow);

    checkRow = new CheckRow();
    checkRow->setWidth(420);
    checkRow->setHeight(48);
    checkRow->setMarginBottom(8);
    column->addView(checkRow);

    for (brls::Label** note : {&reasonLabel, &hintLabel, &noticeLabel})
    {
        *note = new brls::Label();
        (*note)->setFontSize(16);
        (*note)->setTextColor(pal.textMuted);
        (*note)->setWidth(560);
        (*note)->setMarginBottom(6);
        (*note)->setVisibility(brls::Visibility::GONE);
        column->addView(*note);
    }

    loadImage(game.artworkUrl(), kBackdropWidth, backdrop);
}

void GameDetailView::buildBand()
{
    band = new brls::Box();
    band->setAxis(brls::Axis::ROW);
    band->setAlignItems(brls::AlignItems::FLEX_START);
    band->setWidthPercentage(100.0f);
    band->setGrow(1.0f);
    band->setPadding(22, kInset, 18, kInset);
    this->addView(band);
}

void GameDetailView::loadDetails()
{
    auto guard = alive;
    Service::instance().detailsOf(game, lifetime.token(), [this, guard](std::optional<GameDetail> detail) {
        if (!*guard || !detail)
            return;
        showDetail(*detail);
    });
}

void GameDetailView::showDetail(const GameDetail& detail)
{
    if (detail.art != GameDetail::Art::None && !detail.background.empty())
        loadImage(detail.background, kBackdropWidth, backdrop);
    if (detail.art == GameDetail::Art::Layered && !detail.hero.empty())
    {
        hero->setVisibility(brls::Visibility::VISIBLE);
        loadImage(detail.hero, kBackdropWidth, hero);
    }
    if (!detail.logo.empty())
    {
        auto guard = alive;
        Service::instance().imageOf(detail.logo, kLogoWidth, lifetime.token(),
            [this, guard](std::vector<uint8_t> bytes) {
                if (!*guard || bytes.empty())
                    return;
                logo->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
                logo->setVisibility(brls::Visibility::VISIBLE);
                titleLabel->setVisibility(brls::Visibility::GONE);
            });
    }
    renderBand(detail);
}

void GameDetailView::loadImage(const std::string& url, int width, brls::Image* into)
{
    if (url.empty())
        return;
    auto guard = alive;
    Service::instance().imageOf(url, width, lifetime.token(), [guard, into](std::vector<uint8_t> bytes) {
        if (!*guard || bytes.empty())
            return;
        into->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
    });
}

void GameDetailView::startCardCheck()
{
    if (!CardCheck::needed(game) || (cardCheck && cardCheck->isSettled()) || !Service::instance().signedIn())
        return;
    cardCheck = CardCheck::checking();
    render();
    auto guard = alive;
    Service::instance().checkStorefront(game, lifetime.token(), [this, guard](CardCheck answer) {
        if (!*guard)
            return;
        cardCheck = std::move(answer);
        render();
    });
}

auto GameDetailView::actions() const -> GameActions
{
    return GameActions(game, Service::instance().signedIn(), cardCheck, titleRefresh == TitleRefreshPhase::Idle);
}

void GameDetailView::render()
{
    renderMetadata();
    renderActions();
    renderNotes();
}

auto GameDetailView::metadataLabel(const std::string& text, NVGcolor color) -> brls::Label*
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(17);
    label->setTextColor(color);
    label->setMarginRight(26);
    label->setSingleLine(true);
    return label;
}

auto GameDetailView::standingText(Standing standing, const Game& game) -> std::string
{
    if (game.isTrial && standing != Standing::Store && standing != Standing::OwnershipUnknown)
    {
        std::string lower = game.name;
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });
        const bool demo = lower == "demo" || lower.starts_with("demo ") || lower.ends_with(" demo") ||
                          lower.find(" demo ") != std::string::npos;
        return demo ? "akira/cloud/standing_demo"_i18n : "akira/cloud/standing_trial"_i18n;
    }
    switch (standing)
    {
        case Standing::MayStream: return "akira/cloud/standing_may_stream"_i18n;
        case Standing::InLibrary: return "akira/cloud/standing_in_library"_i18n;
        case Standing::Included: return "akira/cloud/standing_included"_i18n;
        case Standing::Store:
        case Standing::OwnershipUnknown: return "akira/cloud/standing_store"_i18n;
    }
    return {};
}

auto GameDetailView::releaseText(const Game& game, int64_t now) -> std::optional<std::string>
{
    switch (game.release)
    {
        case ReleaseKind::Unrestricted: return std::nullopt;
        case ReleaseKind::Unannounced: return "akira/cloud/release_unannounced"_i18n;
        case ReleaseKind::Scheduled: break;
    }
    const int64_t remainingMs = game.availableFromMs - now;
    if (remainingMs <= 0)
        return std::nullopt;
    const int64_t remaining = (remainingMs + 999) / 1000;
    if (remaining < 86400 * 7)
        return brls::getStr("akira/cloud/release_in", Game::countdown(remaining));
    const std::time_t at = static_cast<std::time_t>(game.availableFromMs / 1000);
    std::tm local = *std::localtime(&at);
    char text[64];
    std::strftime(text, sizeof(text), "%d %b %Y %H:%M", &local);
    return brls::getStr("akira/cloud/release_on", std::string(text));
}

void GameDetailView::renderMetadata()
{
    const auto& pal = akira::ui::active();
    metadataRow->clearViews();
    std::string platform = game.platform;
    std::ranges::transform(platform, platform.begin(), [](unsigned char c) { return std::toupper(c); });
    if (!platform.empty())
        metadataRow->addView(metadataLabel(platform, pal.textMuted));
    metadataRow->addView(metadataLabel(standingText(GameActions::standingAfter(game, cardCheck), game),
                                       game.isInCatalog() ? pal.gold : pal.textMuted));
    releaseLabel = metadataLabel("", pal.textMuted);
    releaseLabel->setVisibility(brls::Visibility::GONE);
    metadataRow->addView(releaseLabel);
    releaseShown.clear();
    updateRelease();
}

void GameDetailView::updateRelease()
{
    const auto text = releaseText(game, nowMs());
    const std::string shown = text.value_or("");
    if (shown == releaseShown)
        return;
    releaseShown = shown;
    releaseLabel->setText(shown);
    releaseLabel->setVisibility(text ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

auto GameDetailView::focusedAction() const -> std::optional<GameActions::Focus>
{
    for (brls::View* f = brls::Application::getCurrentFocus(); f; f = f->getParent())
        for (const auto& [focus, button] : buttons)
            if (button == f)
                return focus;
    return std::nullopt;
}

auto GameDetailView::actionButton(GameActions::Focus focus) const -> brls::Button*
{
    for (const auto& [kind, button] : buttons)
        if (kind == focus && button->isFocusable())
            return button;
    return nullptr;
}

void GameDetailView::renderActions()
{
    const GameActions shown = actions();
    const auto previous = focusedAction();
    const bool playWasEnabled = actionButton(GameActions::Focus::Play) != nullptr;
    const bool focusInside = previous.has_value() || brls::Application::getCurrentFocus() == this;
    if (previous)
    {
        parkingFocus = true;
        brls::Application::giveFocus(this);
        parkingFocus = false;
    }

    actionsRow->clearViews();
    buttons.clear();

    const auto add = [this](GameActions::Focus focus, const std::string& text, bool prominent, bool enabled,
                            std::function<void()> act) {
        auto* button = new brls::Button();
        button->setStyle(prominent ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_BORDERED);
        button->setText(text);
        button->setMarginRight(12);
        button->setHeight(48);
        button->setPaddingLeft(22);
        button->setPaddingRight(22);
        if (!enabled)
        {
            button->setState(brls::ButtonState::DISABLED);
            button->setFocusable(false);
        }
        button->registerClickAction([act = std::move(act)](brls::View*) {
            act();
            return true;
        });
        actionsRow->addView(button);
        buttons.emplace_back(focus, button);
    };

    if (const auto& play = shown.play())
    {
        const bool choose = play->offer == GameActions::Offer::ChooseGame;
        const bool running = checks && checks->isRunning();
        add(GameActions::Focus::Play,
            choose ? "akira/cloud/detail_choose_game"_i18n : "akira/cloud/detail_check_and_play"_i18n, true,
            play->enabled() && !running, [this, choose]() {
                if (choose)
                    chooseBundleGame();
                else
                    checkAndPlay(game);
            });
    }
    if (const auto& store = shown.store())
    {
        add(GameActions::Focus::Store,
            *store == GameActions::StoreTitle::ViewInStore ? "akira/cloud/detail_store_qr"_i18n
                                                           : "akira/cloud/detail_store_page_qr"_i18n,
            false, true, [this]() { showStorePage(false); });
    }
    else if (shown.addsToLibrary())
    {
        add(GameActions::Focus::AddToLibrary, "akira/cloud/detail_add_to_library_qr"_i18n, true, true,
            [this]() { showStorePage(true); });
        add(GameActions::Focus::RefreshTitle,
            refreshText(),
            false, shown.refreshTitleFocusable(), [this]() { refreshTitle(); });
    }

    const bool playEnabled = actionButton(GameActions::Focus::Play) != nullptr;
    brls::Button* target = nullptr;
    if (playEnabled && !playWasEnabled && (!previous || previous == GameActions::Focus::Play))
        target = actionButton(GameActions::Focus::Play);
    else if (previous)
        target = actionButton(*previous);
    if (!target && previous == GameActions::Focus::AddToLibrary)
        target = actionButton(GameActions::Focus::RefreshTitle);
    if (!target)
        for (GameActions::Focus focus : shown.focusOrder())
            if ((target = actionButton(focus)))
                break;
    if (focusInside && target)
        brls::Application::giveFocus(target);
}

auto GameDetailView::cardCheckText(const CardCheck& check) -> std::optional<std::string>
{
    switch (check.kind())
    {
        case CardCheck::Kind::Checking: return "akira/cloud/card_check_checking"_i18n;
        case CardCheck::Kind::Streams: return std::nullopt;
        case CardCheck::Kind::NotStreamable: return "akira/cloud/card_check_not_streamable"_i18n;
        case CardCheck::Kind::Refused: return "akira/cloud/card_check_refused"_i18n;
        case CardCheck::Kind::Unavailable: return brls::getStr("akira/cloud/card_check_unavailable", check.detail());
    }
    return std::nullopt;
}

void GameDetailView::renderNotes()
{
    const GameActions shown = actions();
    const auto& pal = akira::ui::active();

    checkRow->show(shown.showsLaunchChecks() && checks ? checks : std::nullopt);
    checkRow->setVisibility(shown.showsLaunchChecks() && checks ? brls::Visibility::VISIBLE
                                                                : brls::Visibility::GONE);

    std::vector<std::string> reasons;
    if (shown.showsLaunchChecks())
    {
        if (checks && checks->reason())
            reasons.push_back(*checks->reason());
        if (cardCheck)
            if (auto text = cardCheckText(*cardCheck))
                reasons.push_back(*text);
    }
    std::string reasonText;
    for (const std::string& reason : reasons)
        reasonText += (reasonText.empty() ? "" : "\n") + reason;
    reasonLabel->setText(reasonText);
    reasonLabel->setVisibility(reasonText.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    hintLabel->setText("akira/cloud/detail_library_hint"_i18n);
    hintLabel->setVisibility(shown.showsLibraryAddHint() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

    const bool notice = shown.showsTitleNotice() && titleNotice;
    noticeLabel->setText(titleNotice.value_or(""));
    noticeLabel->setTextColor(titleNoticeAdded ? pal.success : pal.textMuted);
    noticeLabel->setVisibility(notice ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void GameDetailView::renderBand(const GameDetail& detail)
{
    const auto& pal = akira::ui::active();
    band->clearViews();

    if (detail.rating)
    {
        const GameDetail::Rating& rating = *detail.rating;
        auto* summary = new brls::Box();
        summary->setAxis(brls::Axis::COLUMN);
        summary->setMarginRight(36);

        auto* average = new brls::Label();
        average->setFontSize(44);
        average->setTextColor(pal.text);
        average->setText(rating.averageText.empty() ? std::format("{:.2f}", rating.average) : rating.averageText);
        summary->addView(average);

        const int filled = std::clamp(static_cast<int>(std::lround(rating.average)), 0, 5);
        auto* stars = new brls::Box();
        stars->setAxis(brls::Axis::ROW);
        for (int star = 1; star <= 5; ++star)
        {
            auto* glyph = new brls::Label();
            glyph->setText("\xE2\x98\x85");
            glyph->setFontSize(18);
            glyph->setTextColor(star <= filled ? pal.gold : akira::ui::withAlpha(pal.text, 0x38));
            stars->addView(glyph);
        }
        summary->addView(stars);

        auto* count = new brls::Label();
        count->setFontSize(14);
        count->setTextColor(pal.textDim);
        count->setText(brls::getStr(rating.count == 1 ? "akira/cloud/rating_count_one" : "akira/cloud/rating_count",
                                    rating.count));
        summary->addView(count);
        band->addView(summary);

        auto* bars = new brls::Box();
        bars->setAxis(brls::Axis::COLUMN);
        bars->setWidth(300);
        bars->setMarginRight(36);
        for (int stars = 5; stars >= 1; --stars)
        {
            const int percent = std::clamp(rating.percentByStars[static_cast<std::size_t>(stars - 1)], 0, 100);
            auto* row = new brls::Box();
            row->setAxis(brls::Axis::ROW);
            row->setAlignItems(brls::AlignItems::CENTER);
            row->setMarginBottom(4);

            auto* number = new brls::Label();
            number->setText(std::to_string(stars));
            number->setFontSize(13);
            number->setTextColor(pal.textMuted);
            number->setWidth(16);
            row->addView(number);

            auto* track = new brls::Box();
            track->setWidth(220);
            track->setHeight(6);
            track->setCornerRadius(3);
            track->setBackgroundColor(akira::ui::withAlpha(pal.text, 0x1a));
            auto* fill = new brls::Box();
            fill->setWidth(2.2f * static_cast<float>(percent));
            fill->setHeight(6);
            fill->setCornerRadius(3);
            fill->setBackgroundColor(pal.text);
            track->addView(fill);
            row->addView(track);

            auto* label = new brls::Label();
            label->setText(std::format("{}%", percent));
            label->setFontSize(13);
            label->setTextColor(pal.textDim);
            label->setMarginLeft(10);
            row->addView(label);
            bars->addView(row);
        }
        band->addView(bars);
    }

    if (detail.age)
    {
        auto* age = new brls::Box();
        age->setAxis(brls::Axis::ROW);
        age->setMarginRight(36);
        if (!detail.age->icon.empty())
        {
            auto* icon = new brls::Image();
            icon->setWidth(52);
            icon->setHeight(62);
            icon->setScalingType(brls::ImageScalingType::FIT);
            icon->setMarginRight(12);
            age->addView(icon);
            loadImage(detail.age->icon, 0, icon);
        }
        auto* text = new brls::Box();
        text->setAxis(brls::Axis::COLUMN);
        text->setWidth(200);
        if (!detail.age->name.empty())
        {
            auto* name = new brls::Label();
            name->setText(detail.age->name);
            name->setFontSize(15);
            name->setTextColor(pal.text);
            text->addView(name);
        }
        if (!detail.age->descriptors.empty())
        {
            auto* descriptors = new brls::Label();
            descriptors->setText(detail.age->descriptors);
            descriptors->setFontSize(13);
            descriptors->setTextColor(pal.textDim);
            text->addView(descriptors);
        }
        age->addView(text);
        band->addView(age);
    }

    std::vector<std::pair<std::string, std::string>> facts;
    if (!detail.publisher.empty())
        facts.emplace_back("akira/cloud/fact_publisher"_i18n, detail.publisher);
    if (detail.releasedMs)
    {
        const std::time_t at = static_cast<std::time_t>(*detail.releasedMs / 1000);
        std::tm utc = *std::gmtime(&at);
        char text[64];
        std::strftime(text, sizeof(text), "%d %B %Y", &utc);
        facts.emplace_back("akira/cloud/fact_released"_i18n, text);
    }
    if (!detail.genres.empty())
    {
        std::string genres;
        for (const std::string& genre : detail.genres)
            genres += (genres.empty() ? "" : ", ") + genre;
        facts.emplace_back("akira/cloud/fact_genres"_i18n, genres);
    }
    if (!facts.empty())
    {
        auto* column = new brls::Box();
        column->setAxis(brls::Axis::COLUMN);
        column->setGrow(1.0f);
        for (const auto& [label, value] : facts)
        {
            auto* name = new brls::Label();
            name->setText(label);
            name->setFontSize(12);
            name->setTextColor(pal.textDim);
            column->addView(name);
            auto* fact = new brls::Label();
            fact->setText(value);
            fact->setFontSize(15);
            fact->setTextColor(pal.text);
            fact->setMarginBottom(8);
            column->addView(fact);
        }
        band->addView(column);
    }
}

auto GameDetailView::bundleChoices() const -> std::vector<Game>
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

void GameDetailView::chooseBundleGame()
{
    std::vector<Game> choices = bundleChoices();
    if (choices.empty())
        return;
    std::vector<std::string> names;
    for (const Game& choice : choices)
        names.push_back(choice.name);
    auto guard = alive;
    auto* dropdown = new brls::Dropdown("akira/cloud/detail_choose_game"_i18n, names,
        [this, guard, choices](int selected) {
            if (!*guard || selected < 0 || selected >= static_cast<int>(choices.size()))
                return;
            checkAndPlay(choices[static_cast<std::size_t>(selected)]);
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void GameDetailView::checkAndPlay(const Game& target)
{
    if (checks && checks->isRunning())
        return;
    static_cast<void>(launchStop.request_stop());
    launchStop = libchiaking::platform::StopSource{};
    const uint64_t attempt = ++attempts;
    checks.emplace(target.productId, attempt, game);
    render();

    auto guard = alive;
    const auto current = [this, guard, attempt]() {
        return *guard && checks && checks->attempt() == attempt;
    };
    Service::instance().clearForLaunch(
        target, launchStop.token(),
        [this, current](std::shared_ptr<CloudLaunch> launch) {
            if (!current())
                return;
            checks->clear();
            render();
            akira::views::startConnecting(std::make_unique<CloudConnectTask>(std::move(launch)));
            checks->launched();
        },
        [this, current](const std::string& error) {
            if (!current())
                return;
            checks->refuse(error);
            render();
        },
        [this, current](libchiaking::catalog::LaunchCheck check) {
            if (!current())
                return;
            checks->observe(checkOf(check));
            checkRow->show(checks);
        });
}

void GameDetailView::showStorePage(bool addingToLibrary)
{
    const std::string body = addingToLibrary
        ? brls::getStr("akira/cloud/detail_add_library_body", game.name, game.conceptUrl)
        : brls::getStr("akira/cloud/detail_store_body", game.conceptUrl);
    akira::ui::QrCodeView::openDialog(game.conceptUrl, body);
}

void GameDetailView::refreshTitle()
{
    if (titleRefresh != TitleRefreshPhase::Idle || !game.canBeAddedToLibrary)
        return;
    titleRefresh = TitleRefreshPhase::Checking;
    titleNotice.reset();
    refreshReadyAt = std::chrono::steady_clock::now() + kTitleRefreshCooldown;
    refreshShownSecond = -1;
    const uint64_t attempt = ++refreshAttempt;
    render();

    auto guard = alive;
    Service::instance().refreshTitle(game, lifetime.token(), [this, guard, attempt](TitleRefresh refreshed) {
        if (!*guard || attempt != refreshAttempt || titleRefresh != TitleRefreshPhase::Checking)
            return;
        titleRefresh = refreshSecondsLeft() > 0 ? TitleRefreshPhase::CoolingDown : TitleRefreshPhase::Idle;
        titleNoticeAdded = refreshed.outcome == TitleRefresh::Outcome::Added;
        switch (refreshed.outcome)
        {
            case TitleRefresh::Outcome::Added:
                titleNotice = "akira/cloud/title_added"_i18n;
                if (refreshed.game)
                    game = *refreshed.game;
                break;
            case TitleRefresh::Outcome::NotInLibrary:
                titleNotice = "akira/cloud/title_not_in_library"_i18n;
                break;
            case TitleRefresh::Outcome::Failed:
                titleNotice = refreshed.message;
                break;
        }
        render();
    });
}

void GameDetailView::tickRefreshCooldown()
{
    if (titleRefresh != TitleRefreshPhase::CoolingDown)
        return;
    const int left = refreshSecondsLeft();
    if (left <= 0)
    {
        titleRefresh = TitleRefreshPhase::Idle;
        auto guard = alive;
        brls::sync([this, guard]() {
            if (*guard)
                render();
        });
        return;
    }
    if (left == refreshShownSecond)
        return;
    refreshShownSecond = left;
    for (const auto& [kind, button] : buttons)
        if (kind == GameActions::Focus::RefreshTitle)
            button->setText(refreshText());
}

auto GameDetailView::refreshText() const -> std::string
{
    switch (titleRefresh)
    {
        case TitleRefreshPhase::Checking:
            return "akira/cloud/detail_refresh_checking"_i18n;
        case TitleRefreshPhase::CoolingDown:
            return brls::getStr("akira/cloud/detail_refresh_wait", refreshSecondsLeft());
        case TitleRefreshPhase::Idle:
            break;
    }
    return "akira/cloud/detail_refresh"_i18n;
}

auto GameDetailView::refreshSecondsLeft() const -> int
{
    const auto left = refreshReadyAt - std::chrono::steady_clock::now();
    if (left <= std::chrono::steady_clock::duration::zero())
        return 0;
    return static_cast<int>(std::chrono::ceil<std::chrono::seconds>(left).count());
}

brls::View* GameDetailView::getDefaultFocus()
{
    if (parkingFocus)
        return this;
    for (GameActions::Focus focus : actions().focusOrder())
        if (brls::Button* button = actionButton(focus))
            return button;
    return this;
}

void GameDetailView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                          brls::FrameContext* ctx)
{
    updateRelease();
    tickRefreshCooldown();
    Box::draw(vg, x, y, width, height, style, ctx);
}

void GameDetailView::StageShade::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style,
                                      brls::FrameContext*)
{
    const NVGcolor page = akira::ui::active().backgroundDeep;
    const auto shaded = [&](float alpha) { return akira::ui::withAlpha(page, static_cast<unsigned char>(alpha * 255)); };
    const auto band = [&](NVGpaint paint, float bx, float by, float bw, float bh) {
        nvgBeginPath(vg);
        nvgRect(vg, bx, by, bw, bh);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    };

    band(nvgLinearGradient(vg, x, 0, x + width * 0.26f, 0, shaded(0.88f), shaded(0.62f)), x, y, width * 0.26f, height);
    band(nvgLinearGradient(vg, x + width * 0.26f, 0, x + width * 0.56f, 0, shaded(0.62f), shaded(0.0f)),
         x + width * 0.26f, y, width * 0.30f, height);

    const float bottom = y + height;
    band(nvgLinearGradient(vg, 0, bottom, 0, bottom - height * 0.16f, shaded(1.0f), shaded(0.55f)), x,
         bottom - height * 0.16f, width, height * 0.16f);
    band(nvgLinearGradient(vg, 0, bottom - height * 0.16f, 0, bottom - height * 0.4f, shaded(0.55f), shaded(0.0f)),
         x, bottom - height * 0.4f, width, height * 0.24f);
}

GameDetailView::CheckRow::CheckRow()
{
    this->setFocusable(false);
}

void GameDetailView::CheckRow::show(std::optional<LaunchChecks> checks)
{
    shown = std::move(checks);
}

auto GameDetailView::CheckRow::label(LaunchChecks::Step step) -> std::string
{
    switch (step)
    {
        case LaunchChecks::Step::Membership: return "akira/cloud/step_membership"_i18n;
        case LaunchChecks::Step::Premium: return "akira/cloud/step_premium"_i18n;
        case LaunchChecks::Step::YourCopy: return "akira/cloud/step_your_copy"_i18n;
        case LaunchChecks::Step::CloudGame: return "akira/cloud/step_cloud_game"_i18n;
        case LaunchChecks::Step::Restrictions: return "akira/cloud/step_restrictions"_i18n;
        case LaunchChecks::Step::Release: return "akira/cloud/step_release"_i18n;
    }
    return {};
}

void GameDetailView::CheckRow::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style,
                                    brls::FrameContext*)
{
    if (!shown)
        return;
    const auto& pal = akira::ui::active();
    const auto& steps = shown->steps();
    const float slot = width / static_cast<float>(steps.size());
    const float radius = 9.0f;

    for (std::size_t i = 0; i < steps.size(); ++i)
    {
        const float cx = x + slot * (static_cast<float>(i) + 0.5f);
        const float cy = y + radius + 1.0f;
        const LaunchChecks::Mark mark = shown->mark(steps[i]);

        nvgBeginPath(vg);
        switch (mark)
        {
            case LaunchChecks::Mark::Unchecked:
                nvgCircle(vg, cx, cy, radius);
                nvgStrokeWidth(vg, 2.0f);
                nvgStrokeColor(vg, pal.textDim);
                nvgStroke(vg);
                break;
            case LaunchChecks::Mark::Checking:
            {
                const float t = static_cast<float>(brls::getCPUTimeUsec() % 1000000) / 1000000.0f;
                const float a0 = t * 2.0f * NVG_PI;
                nvgArc(vg, cx, cy, radius, a0, a0 + NVG_PI * 1.2f, NVG_CW);
                nvgStrokeWidth(vg, 2.5f);
                nvgStrokeColor(vg, pal.accent);
                nvgStroke(vg);
                break;
            }
            case LaunchChecks::Mark::Passed:
                nvgCircle(vg, cx, cy, radius);
                nvgFillColor(vg, pal.success);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgMoveTo(vg, cx - 4.5f, cy);
                nvgLineTo(vg, cx - 1.0f, cy + 3.5f);
                nvgLineTo(vg, cx + 5.0f, cy - 3.5f);
                nvgStrokeWidth(vg, 2.0f);
                nvgStrokeColor(vg, pal.backgroundDeep);
                nvgStroke(vg);
                break;
            case LaunchChecks::Mark::Refused:
                nvgCircle(vg, cx, cy, radius);
                nvgFillColor(vg, pal.danger);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgMoveTo(vg, cx - 3.5f, cy - 3.5f);
                nvgLineTo(vg, cx + 3.5f, cy + 3.5f);
                nvgMoveTo(vg, cx + 3.5f, cy - 3.5f);
                nvgLineTo(vg, cx - 3.5f, cy + 3.5f);
                nvgStrokeWidth(vg, 2.0f);
                nvgStrokeColor(vg, pal.backgroundDeep);
                nvgStroke(vg);
                break;
        }

        nvgFontSize(vg, 13.0f);
        nvgFillColor(vg, mark == LaunchChecks::Mark::Unchecked ? pal.textDim : pal.text);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        const std::string text = label(steps[i]);
        nvgText(vg, cx, cy + radius + 6.0f, text.c_str(), nullptr);
    }
}

} // namespace cloud
