#include "cloud/library_view.hpp"

#include "cloud/game_detail_view.hpp"

#include "core/settings_manager.hpp"
#include "core/trophy_manager.hpp"
#include "ui/motion.hpp"
#include "ui/qr_code_view.hpp"
#include "ui/theme.hpp"
#include "util/shared_view_holder.hpp"
#include "views/qr_sign_in_view.hpp"
#include "views/stream_view.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

using namespace brls::literals;

namespace cloud {

static constexpr size_t kCloudPerRow = 4;
static constexpr float kCloudRowHeight = 258.0f;

static brls::Box* makePill(const std::string& text, NVGcolor color)
{
    auto* pill = new brls::Box();
    pill->setAxis(brls::Axis::ROW);
    pill->setCornerRadius(9);
    pill->setPaddingLeft(9);
    pill->setPaddingRight(9);
    pill->setPaddingTop(1);
    pill->setPaddingBottom(2);
    pill->setMarginRight(6);
    pill->setBackgroundColor(akira::ui::withAlpha(color, 0x2e));

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(14);
    label->setTextColor(color);
    pill->addView(label);
    return pill;
}

class CloudGameCard;
static std::vector<CloudGameCard*> g_liveCloudCards;

class CloudGameCard : public brls::Box {
public:
    CloudGameCard()
    {
        this->setAxis(brls::Axis::COLUMN);
        this->setFocusable(false);
        this->setCornerRadius(14);
        this->setPadding(12, 12, 12, 12);
        this->setBackgroundColor(akira::ui::active().surface);

        cover = new brls::Image();
        cover->setHeight(178);
        cover->setWidthPercentage(100.0f);
        cover->setCornerRadius(10);
        cover->setScalingType(brls::ImageScalingType::FILL);
        cover->setBackgroundColor(akira::ui::active().surfaceElevated);
        this->addView(cover);

        name = new brls::Label();
        name->setFontSize(20);
        name->setMarginTop(10);
        name->setWidthPercentage(100.0f);
        name->setSingleLine(true);
        this->addView(name);

        pillRow = new brls::Box();
        pillRow->setAxis(brls::Axis::ROW);
        pillRow->setMarginTop(8);
        this->addView(pillRow);

        this->registerClickAction([this](brls::View*) {
            if (boundGame.launchLeftOut)
                brls::Application::notify("akira/cloud/launch_not_built"_i18n);
            else if (launchCb)
                launchCb(boundGame);
            return true;
        });

        this->registerAction("akira/cloud/pin_shortcut"_i18n, brls::ControllerButton::BUTTON_X,
            [this](brls::View*) {
                if (boundGame.launchable() && pinCb)
                    pinCb(boundGame);
                return true;
            });

        akira::ui::motion::liftOnFocus(this, focusAnim);
        g_liveCloudCards.push_back(this);
    }

    ~CloudGameCard() override
    {
        g_liveCloudCards.erase(
            std::remove(g_liveCloudCards.begin(), g_liveCloudCards.end(), this),
            g_liveCloudCards.end());
    }

    void bind(const Game& game, const std::function<void(const Game&)>& onLaunch,
        const std::function<void(const Game&)>& onPin)
    {
        const auto& pal = akira::ui::active();
        bool streamable = game.streamableNow() && !game.launchLeftOut;

        boundGame = game;
        launchCb = onLaunch;
        pinCb = onPin;

        this->setVisibility(brls::Visibility::VISIBLE);
        this->setFocusable(true);

        name->setText(game.name);
        name->setTextColor(streamable ? pal.text : pal.textMuted);
        cover->setAlpha(streamable ? 1.0f : 0.5f);
        cover->clear();

        pillRow->setAlpha(game.launchLeftOut ? 0.5f : 1.0f);
        pillRow->clearViews();
        std::string platform = platformBadge(game);
        if (!platform.empty())
        {
            NVGcolor pc = game.platform == "ps5" ? pal.accentStrong
                        : game.platform == "ps4" ? pal.textMuted
                                                  : pal.media;
            pillRow->addView(makePill(platform, pc));
        }
        if (game.plusCatalog)
            pillRow->addView(makePill("akira/cloud/badge_plus"_i18n, pal.gold));
        if (game.serviceType == "pscloud")
        {
            if (game.streamabilityStatus == StreamabilityStatus::Streamable)
                pillRow->addView(makePill("akira/cloud/badge_verified"_i18n, pal.success));
            else if (game.streamabilityStatus == StreamabilityStatus::NotStreamable)
                pillRow->addView(makePill("akira/cloud/badge_unavailable"_i18n, pal.danger));
            else
                pillRow->addView(makePill("akira/cloud/badge_unverified"_i18n, pal.textMuted));
        }
        std::string category = categoryBadge(game);
        if (!category.empty())
        {
            NVGcolor cc = game.isOwned ? pal.success
                        : game.category == "streamable" ? pal.media
                                                         : pal.warning;
            pillRow->addView(makePill(category, cc));
        }

        iconUrl = game.artworkUrl();
        if (iconUrl.empty())
            return;

        TrophyManager::getInstance()->fetchIcon(iconUrl,
            [](const std::string& url, const std::vector<uint8_t>& bytes) {
                if (bytes.empty())
                    return;
                for (CloudGameCard* card : g_liveCloudCards)
                    if (card->iconUrl == url)
                        card->cover->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
            });
    }

    void reset()
    {
        this->setVisibility(brls::Visibility::INVISIBLE);
        this->setFocusable(false);
        launchCb = nullptr;
        iconUrl.clear();
        cover->clear();
        pillRow->clearViews();
    }

    std::string iconUrl;
    brls::Image* cover = nullptr;
    brls::Label* name = nullptr;
    brls::Box* pillRow = nullptr;

private:
    Game boundGame;
    std::function<void(const Game&)> launchCb;
    std::function<void(const Game&)> pinCb;
    brls::Animatable focusAnim{0.0f};
};

class CloudGameRowCell : public RecyclingGridItem {
public:
    CloudGameRowCell()
    {
        this->setAxis(brls::Axis::ROW);
        this->setWidthPercentage(100.0f);
        this->setFocusable(false);

        for (size_t i = 0; i < kCloudPerRow; i++)
        {
            cards[i] = new CloudGameCard();
            cards[i]->setGrow(1.0f);
            if (i > 0)
                cards[i]->setMarginLeft(16);
            this->addView(cards[i]);
        }
    }

    static RecyclingGridItem* create()
    {
        return new CloudGameRowCell();
    }

    void bindRow(const std::vector<Game>& games, size_t startIndex,
        const std::function<void(const Game&)>& onLaunch,
        const std::function<void(const Game&)>& onPin)
    {
        for (size_t i = 0; i < kCloudPerRow; i++)
        {
            size_t idx = startIndex + i;
            if (idx < games.size())
                cards[i]->bind(games[idx], onLaunch, onPin);
            else
                cards[i]->reset();
        }
    }

    void prepareForReuse() override
    {
        for (size_t i = 0; i < kCloudPerRow; i++)
            if (cards[i])
                cards[i]->reset();
    }

private:
    CloudGameCard* cards[kCloudPerRow] = {};
};

class CloudCatalogDataSource : public RecyclingGridDataSource {
public:
    CloudCatalogDataSource(std::vector<Game> games, std::function<void(const Game&)> onLaunch,
        std::function<void(const Game&)> onPin)
        : games(std::move(games)), onLaunch(std::move(onLaunch)), onPin(std::move(onPin))
    {
    }

    size_t getItemCount() override
    {
        return (games.size() + kCloudPerRow - 1) / kCloudPerRow;
    }

    float heightForRow(brls::View* recycler, size_t index) override
    {
        return kCloudRowHeight;
    }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override
    {
        auto* cell = dynamic_cast<CloudGameRowCell*>(recycler->dequeueReusableCell("CloudRow"));
        if (!cell)
            return nullptr;
        cell->bindRow(games, index * kCloudPerRow, onLaunch, onPin);
        return cell;
    }

    void clearData() override
    {
        games.clear();
    }

private:
    std::vector<Game> games;
    std::function<void(const Game&)> onLaunch;
    std::function<void(const Game&)> onPin;
};

LibraryView::LibraryView()
{
    if (auto saved = LibraryListing::filterNamed(SettingsManager::getInstance()->getCloudFilter()))
        listing.pick(*saved);

    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(22, 26, 22, 26);

    auto* header = new brls::Box();
    header->setAxis(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    header->setWidthPercentage(100.0f);
    header->setMarginBottom(18);
    this->addView(header);

    auto* headerText = new brls::Box();
    headerText->setAxis(brls::Axis::ROW);
    headerText->setAlignItems(brls::AlignItems::CENTER);
    headerText->setGrow(1.0f);
    header->addView(headerText);

    auto* actions = new brls::Box();
    actions->setAxis(brls::Axis::ROW);
    actions->setAlignItems(brls::AlignItems::CENTER);
    header->addView(actions);

    statusChip = new brls::Box();
    statusChip->setAxis(brls::Axis::ROW);
    statusChip->setCornerRadius(11);
    statusChip->setPaddingLeft(12);
    statusChip->setPaddingRight(12);
    statusChip->setPaddingTop(4);
    statusChip->setPaddingBottom(5);

    statusChipLabel = new brls::Label();
    statusChipLabel->setFontSize(16);
    statusChip->addView(statusChipLabel);
    headerText->addView(statusChip);

    countLabel = new brls::Label();
    countLabel->setFontSize(16);
    countLabel->setMarginLeft(10);
    countLabel->setVisibility(brls::Visibility::GONE);
    headerText->addView(countLabel);

    searchButton = new brls::Button();
    searchButton->setMarginRight(8);
    searchButton->registerClickAction([this](brls::View*) {
        auto guard = alive;
        brls::Application::getImeManager()->openForText(
            [this, guard](std::string text) {
                if (!*guard)
                    return;
                listing.setQuery(std::move(text));
                updateSearchButton();
                applyFilter();
            },
            "akira/cloud/search"_i18n, "", 64, listing.query());
        return true;
    });
    actions->addView(searchButton);
    updateSearchButton();

    sortState = SettingsManager::getInstance()->getCloudSortState();

    sortButton = new brls::Button();
    sortButton->setMarginRight(8);
    sortButton->registerClickAction([this](brls::View*) {
        openSortPicker();
        return true;
    });
    actions->addView(sortButton);
    updateSortButton();

    overflowButton = new brls::Button();
    overflowButton->setText("\xEE\x97\x93");
    overflowButton->registerClickAction([this](brls::View*) {
        openOverflowMenu();
        return true;
    });
    actions->addView(overflowButton);

    auto* controls = controlsRow = new brls::Box();
    controls->setAxis(brls::Axis::ROW);
    controls->setAlignItems(brls::AlignItems::CENTER);
    controls->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    controls->setWidthPercentage(100.0f);
    controls->setMarginBottom(14);
    controls->setVisibility(brls::Visibility::GONE);
    this->addView(controls);

    filterRow = new brls::Box();
    filterRow->setAxis(brls::Axis::ROW);
    filterRow->setAlignItems(brls::AlignItems::CENTER);
    filterRow->setGrow(1.0f);
    controls->addView(filterRow);

    platformRow = new brls::Box();
    platformRow->setAxis(brls::Axis::ROW);
    platformRow->setAlignItems(brls::AlignItems::CENTER);
    controls->addView(platformRow);
    for (PlatformFilter platform : LibraryListing::allPlatforms)
    {
        auto* chip = new brls::Button();
        chip->setText(platformTitle(platform));
        chip->setMarginLeft(6);
        chip->registerClickAction([this, platform](brls::View*) {
            pickPlatform(platform);
            return true;
        });
        platformRow->addView(chip);
        platformChips.push_back(chip);
    }

    grid = new RecyclingGrid();
    grid->setGrow(1.0f);
    grid->registerCell("CloudRow", CloudGameRowCell::create);
    grid->estimatedRowHeight = kCloudRowHeight;
    grid->spanCount = 1;
    grid->isFlowMode = true;
    this->addView(grid);

    stateBox = new brls::Box();
    stateBox->setAxis(brls::Axis::COLUMN);
    stateBox->setGrow(1.0f);
    stateBox->setWidthPercentage(100.0f);
    stateBox->setVisibility(brls::Visibility::GONE);
    this->addView(stateBox);
}

LibraryView::~LibraryView()
{
    *alive = false;
}

void LibraryView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    refresh(false);
}

void LibraryView::refresh(bool force)
{
    const uint64_t gen = ++generation;
    auto guard = alive;

    brls::Logger::info("CloudLib: refresh(force={}) gen={}", force, gen);
    fetch.follow(gen);
    renderSnapshot(Service::instance().snapshotForActiveProfile());
    Service::instance().refreshActiveProfile(force,
        [this, guard, gen](const Snapshot& snapshot) {
            brls::Logger::info("CloudLib: refresh cb gen={} cur={} alive={} avail={} hasCatalog={} games={}",
                gen, generation, *guard, static_cast<int>(snapshot.status.availability),
                snapshot.hasCatalog, snapshot.catalog.games.size());
            if (!*guard || gen != generation)
                return;
            renderSnapshot(snapshot);
        },
        [this, guard, gen](const FetchProgress& progress) {
            if (*guard)
                showFetch(gen, progress);
        });
}

void LibraryView::showFetch(uint64_t operation, const FetchProgress& progress)
{
    const FollowedFetch::Change change = fetch.observe(operation, progress);
    if (change == FollowedFetch::Change::Dropped || change == FollowedFetch::Change::Unchanged)
        return;

    if (progress.shown() == FetchProgress::Shown::Failure)
    {
        renderSnapshot(Service::snapshotOf(*progress.failure()));
        return;
    }

    if (!loadingLines)
        return;
    if (change == FollowedFetch::Change::Step)
        akira::ui::motion::crossfade(loadingLines->box, stepFade, [this]() { writeLoadingLines(); });
    else if (!stepFade.isRunning())
        writeLoadingLines();
}

void LibraryView::addLoadingLines(brls::Box* panel, const std::string& waitingText)
{
    const auto& pal = akira::ui::active();

    auto* box = new brls::Box();
    box->setAxis(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setMarginBottom(18);

    auto* step = new brls::Label();
    step->setFontSize(16);
    step->setTextColor(pal.textDim);
    step->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    step->setWidth(560);
    box->addView(step);

    auto* detail = new brls::Label();
    detail->setFontSize(15);
    detail->setTextColor(pal.textMuted);
    detail->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    detail->setWidth(560);
    detail->setMarginTop(6);
    box->addView(detail);

    auto* pages = new brls::Label();
    pages->setFontSize(14);
    pages->setTextColor(pal.textMuted);
    pages->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    pages->setWidth(560);
    pages->setMarginTop(2);
    box->addView(pages);

    panel->addView(box);
    loadingLines = LoadingLines{box, step, detail, pages, waitingText};
    writeLoadingLines();
}

void LibraryView::writeLoadingLines()
{
    if (!loadingLines)
        return;

    const std::optional<FetchProgress>& progress = fetch.latest();
    const auto stepKey = progress ? progress->stepKey() : std::nullopt;
    const auto detailKey = progress ? progress->detailKey() : std::nullopt;
    const auto pageKey = progress ? progress->pageKey() : std::nullopt;

    loadingLines->step->setText(stepKey ? brls::getStr(std::string(*stepKey)) : loadingLines->waitingText);

    loadingLines->detail->setVisibility(detailKey ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (detailKey && progress->timeServer())
        loadingLines->detail->setText(brls::getStr(std::string(*detailKey), *progress->timeServer()));
    else if (detailKey)
        loadingLines->detail->setText(brls::getStr(std::string(*detailKey)));

    loadingLines->pages->setVisibility(pageKey ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (pageKey && progress->pagesExpected())
        loadingLines->pages->setText(brls::getStr(std::string(*pageKey), progress->pagesRead(), *progress->pagesExpected()));
    else if (pageKey)
        loadingLines->pages->setText(brls::getStr(std::string(*pageKey), progress->pagesRead()));
}

void LibraryView::dropLoadingLines()
{
    stepFade.stop();
    loadingLines.reset();
}

void LibraryView::renderSnapshot(const Snapshot& snapshot)
{
    const auto& pal = akira::ui::active();
    const Status& status = snapshot.status;

    canPairState = status.canPair;
    checkingState = status.availability == Availability::Checking;

    std::string chipText;
    NVGcolor chipColor = pal.textDim;
    bool chipShown = true;
    switch (status.availability)
    {
        case Availability::Ready:
            chipText = "akira/cloud/chip_ready"_i18n;
            chipColor = pal.success;
            break;
        case Availability::CatalogAvailable:
            chipText = "akira/cloud/chip_catalog"_i18n;
            chipColor = pal.warning;
            break;
        case Availability::SubscriptionRequired:
            chipText = "akira/cloud/chip_unavailable"_i18n;
            chipColor = pal.danger;
            break;
        case Availability::Warning:
        case Availability::LaunchBlocked:
            chipText = "akira/cloud/chip_warning"_i18n;
            chipColor = pal.warning;
            break;
        case Availability::Empty:
            chipText = "akira/cloud/chip_empty"_i18n;
            chipColor = pal.textDim;
            break;
        case Availability::NeedsPairing:
            chipText = "akira/cloud/chip_pair"_i18n;
            chipColor = pal.accent;
            break;
        case Availability::Checking:
            chipText = "akira/cloud/chip_checking"_i18n;
            chipColor = pal.accent;
            break;
        case Availability::Error:
            chipText = "akira/cloud/chip_error"_i18n;
            chipColor = pal.danger;
            break;
        default:
            chipShown = false;
            break;
    }

    statusChip->setVisibility(chipShown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (chipShown)
    {
        statusChipLabel->setText(chipText);
        statusChipLabel->setTextColor(chipColor);
        statusChip->setBackgroundColor(akira::ui::withAlpha(chipColor, 0x2e));
    }

    (void)pal;

    if (!snapshot.hasCatalog || snapshot.catalog.games.empty())
    {
        brls::Logger::info("CloudLib: renderSnapshot -> STATE (avail={} hasCatalog={} games={})",
            static_cast<int>(status.availability), snapshot.hasCatalog, snapshot.catalog.games.size());
        grid->setVisibility(brls::Visibility::GONE);
        grid->clearData();
        controlsRow->setVisibility(brls::Visibility::GONE);
        listing.setGames({});
        if (countLabel)
            countLabel->setVisibility(brls::Visibility::GONE);
        showState(snapshot);
        return;
    }

    brls::Logger::info("CloudLib: renderSnapshot -> CATALOG ({} games)", snapshot.catalog.games.size());

    bool refocusGrid = false;
    for (brls::View* f = brls::Application::getCurrentFocus(); f; f = f->getParent())
    {
        if (f == stateBox)
        {
            refocusGrid = true;
            break;
        }
    }

    dropLoadingLines();
    stateBox->clearViews();
    stateBox->setVisibility(brls::Visibility::GONE);
    grid->setVisibility(brls::Visibility::VISIBLE);
    controlsRow->setVisibility(brls::Visibility::VISIBLE);
    showCatalog(snapshot);

    if (refocusGrid)
        brls::Application::giveFocus(grid);
}

void LibraryView::showState(const Snapshot& snapshot)
{
    const auto& pal = akira::ui::active();
    const Status& st = snapshot.status;

    bool refocus = false;
    for (brls::View* f = brls::Application::getCurrentFocus(); f; f = f->getParent())
    {
        if (f == stateBox)
        {
            refocus = true;
            break;
        }
    }

    dropLoadingLines();
    stateBox->clearViews();
    stateBox->setVisibility(brls::Visibility::VISIBLE);

    std::string glyph = "\xEE\x8A\xBD";
    NVGcolor glyphColor = pal.accent;
    std::string title = st.title;
    std::string detail = st.detail;
    enum StateAction { ActNone, ActPair, ActRefresh } action = ActNone;

    switch (st.availability)
    {
        case Availability::NeedsPairing:
            glyph = "\xEE\x85\x97";
            action = ActPair;
            break;
        case Availability::Empty:
            glyphColor = pal.textDim;
            title = "akira/cloud/empty_none_title"_i18n;
            detail = "akira/cloud/empty_none_detail"_i18n;
            action = ActRefresh;
            break;
        case Availability::Error:
        case Availability::LaunchBlocked:
            glyph = "\xEE\x8B\x81";
            glyphColor = pal.danger;
            action = ActRefresh;
            break;
        case Availability::Checking:
            glyphColor = pal.accent;
            break;
        default:
            glyphColor = pal.textDim;
            break;
    }

    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setAlignItems(brls::AlignItems::CENTER);
    panel->setJustifyContent(brls::JustifyContent::CENTER);
    panel->setGrow(1.0f);
    panel->setWidthPercentage(100.0f);

    auto* g = new brls::Label();
    g->setText(glyph);
    g->setFontSize(60);
    g->setTextColor(glyphColor);
    g->setMarginBottom(16);
    panel->addView(g);

    auto* t = new brls::Label();
    t->setText(title);
    t->setFontSize(24);
    t->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    t->setMarginBottom(8);
    panel->addView(t);

    if (st.availability == Availability::Checking)
        addLoadingLines(panel, detail);
    else if (!detail.empty())
    {
        auto* d = new brls::Label();
        d->setText(detail);
        d->setFontSize(16);
        d->setTextColor(pal.textDim);
        d->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        d->setWidth(560);
        d->setMarginBottom(18);
        panel->addView(d);
    }

    brls::Button* actionBtn = nullptr;
    if (action == ActPair)
    {
        actionBtn = new brls::Button();
        actionBtn->setText("akira/cloud/pair"_i18n);
        actionBtn->registerClickAction([this](brls::View*) { openSignIn(); return true; });
        panel->addView(actionBtn);
    }
    else if (action == ActRefresh)
    {
        actionBtn = new brls::Button();
        actionBtn->setText("akira/cloud/check_again"_i18n);
        actionBtn->registerClickAction([this](brls::View*) { refresh(true); return true; });
        panel->addView(actionBtn);
    }

    stateBox->addView(panel);

    if (refocus)
        brls::Application::giveFocus(actionBtn ? static_cast<brls::View*>(actionBtn)
                                               : static_cast<brls::View*>(searchButton));
}

void LibraryView::showCatalog(const Snapshot& snapshot)
{
    const auto systemNow = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    listing.setNow(Service::instance().trustedNowMs().value_or(systemNow));
    listing.setGames(snapshot.catalog.games);
    listing.setRecentlyPlayed(snapshot.recentlyPlayed);
    listing.settle(true);
    scheduleUnlock();
    applyFilter();
}

void LibraryView::scheduleUnlock()
{
    const uint64_t scheduled = ++unlockGeneration;
    const auto next = listing.nextUnlock();
    if (!next)
        return;
    constexpr int64_t margin = 2000;
    constexpr int64_t longest = 24LL * 3600 * 1000;
    const int64_t wait = std::min(*next - listing.now() + margin, longest);
    auto guard = alive;
    brls::delay(static_cast<long>(wait), [this, guard, scheduled]() {
        if (!*guard || scheduled != unlockGeneration)
            return;
        brls::Logger::info("CloudLib: a scheduled release is due, reloading the library");
        refresh(true);
    });
}

auto LibraryView::filterTitle(LibraryFilter filter) -> std::string
{
    switch (filter)
    {
        case LibraryFilter::Recent: return "akira/cloud/filter_recent"_i18n;
        case LibraryFilter::All: return "akira/cloud/filter_all"_i18n;
        case LibraryFilter::Owned: return "akira/cloud/filter_owned"_i18n;
        case LibraryFilter::Plus: return "akira/cloud/filter_plus"_i18n;
        case LibraryFilter::Browse: return "akira/cloud/filter_browse"_i18n;
        case LibraryFilter::Preorders: return "akira/cloud/filter_preorders"_i18n;
    }
    return "akira/cloud/filter_all"_i18n;
}

auto LibraryView::platformTitle(PlatformFilter platform) -> std::string
{
    switch (platform)
    {
        case PlatformFilter::All: return "akira/cloud/platform_all"_i18n;
        case PlatformFilter::PS5: return "PS5";
        case PlatformFilter::PS4: return "PS4";
        case PlatformFilter::PS3: return "PS3";
    }
    return "akira/cloud/platform_all"_i18n;
}

void LibraryView::styleChip(brls::Button* chip, bool selected)
{
    const auto& pal = akira::ui::active();
    chip->setStyle(&brls::BUTTONSTYLE_BORDERLESS);
    chip->setCornerRadius(16);
    chip->setHeight(36);
    chip->setPaddingLeft(14);
    chip->setPaddingRight(14);
    chip->setFontSize(16);
    chip->setTextColor(selected ? pal.accent : pal.textMuted);
    chip->setBackgroundColor(selected ? akira::ui::withAlpha(pal.accent, 0x40) : pal.surface);
}

void LibraryView::rebuildFilterControls()
{
    const std::vector<LibraryFilter> filters = listing.filters();
    if (filters != shownFilters)
    {
        bool focusInside = false;
        for (brls::View* f = brls::Application::getCurrentFocus(); f; f = f->getParent())
            if (f == filterRow)
                focusInside = true;
        if (focusInside)
            brls::Application::giveFocus(searchButton);

        filterRow->clearViews();
        filterChips.clear();
        for (LibraryFilter filter : filters)
        {
            auto* chip = new brls::Button();
            chip->setText(filterTitle(filter));
            chip->setMarginRight(8);
            chip->registerClickAction([this, filter](brls::View*) {
                pickFilter(filter);
                return true;
            });
            filterRow->addView(chip);
            filterChips.push_back(chip);
        }
        shownFilters = filters;

        auto selected = std::ranges::find(shownFilters, listing.filter());
        if (focusInside && selected != shownFilters.end())
            brls::Application::giveFocus(filterChips[static_cast<std::size_t>(selected - shownFilters.begin())]);
    }

    for (std::size_t i = 0; i < filterChips.size(); ++i)
        styleChip(filterChips[i], shownFilters[i] == listing.filter());
    for (std::size_t i = 0; i < platformChips.size(); ++i)
        styleChip(platformChips[i], LibraryListing::allPlatforms[i] == listing.platform());
}

void LibraryView::pickFilter(LibraryFilter filter)
{
    listing.pick(filter);
    auto* settings = SettingsManager::getInstance();
    settings->setCloudFilter(std::string(LibraryListing::key(filter)));
    settings->writeFile();
    applyFilter();
}

void LibraryView::pickPlatform(PlatformFilter platform)
{
    listing.setPlatform(platform);
    applyFilter();
}

void LibraryView::updateSearchButton()
{
    if (!searchButton)
        return;
    searchButton->setText(listing.query().empty() ? "akira/cloud/search"_i18n
                                                  : brls::getStr("akira/cloud/search_for", listing.query()));
}

void LibraryView::applyFilter()
{
    std::vector<Game> filtered = listing.matching();

    auto ciless = [](const std::string& a, const std::string& b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
            [](unsigned char c1, unsigned char c2) { return std::tolower(c1) < std::tolower(c2); });
    };
    const bool recencyOrder = listing.filter() == LibraryFilter::Recent && listing.query().empty();
    if (!recencyOrder && sortState == 1)
        std::sort(filtered.begin(), filtered.end(),
            [&ciless](const Game& a, const Game& b) { return ciless(a.name, b.name); });
    else if (!recencyOrder && sortState == 2)
        std::sort(filtered.begin(), filtered.end(),
            [&ciless](const Game& a, const Game& b) { return ciless(b.name, a.name); });
    else if (!recencyOrder)
        std::stable_sort(filtered.begin(), filtered.end(),
            [](const Game& a, const Game& b) { return a.streamableNow() && !b.streamableNow(); });

    rebuildFilterControls();

    const std::size_t listed = listing.listed().size();
    if (countLabel)
    {
        countLabel->setVisibility(listed == 0 ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        countLabel->setTextColor(akira::ui::active().textDim);
        countLabel->setText(filtered.size() != listed
            ? brls::getStr("akira/cloud/count_filtered", filtered.size(), listed)
            : brls::getStr("akira/cloud/count_all", listed));
    }

    brls::Logger::info("CloudLib: applyFilter tab={} platform={} q='{}' -> {}/{} games",
        LibraryListing::key(listing.filter()), static_cast<int>(listing.platform()), listing.query(),
        filtered.size(), listed);

    auto guard = alive;
    std::function<void(const Game&)> onLaunch = [this, guard](const Game& game) {
        if (*guard)
            openDetail(game);
    };
    std::function<void(const Game&)> onPin = [this, guard](const Game& game) {
        if (*guard)
            toggleShortcut(game);
    };

    auto* source = new CloudCatalogDataSource(std::move(filtered), std::move(onLaunch), std::move(onPin));
    dataSource = source;
    grid->setDataSource(source);
}

void LibraryView::openSortPicker()
{
    std::vector<std::string> names = {
        "akira/cloud/sort_playable"_i18n,
        "akira/cloud/sort_az"_i18n,
        "akira/cloud/sort_za"_i18n,
    };
    auto guard = alive;
    auto* dropdown = new brls::Dropdown(
        "akira/cloud/sort"_i18n, names,
        [this, guard](int sel) {
            if (!*guard)
                return;
            sortState = sel;
            SettingsManager::getInstance()->setCloudSortState(sortState);
            SettingsManager::getInstance()->writeFile();
            updateSortButton();
            applyFilter();
        },
        sortState);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void LibraryView::openOverflowMenu()
{
    enum Op { Refresh, Pair, ClearCache };

    std::vector<std::string> names;
    std::vector<int> ops;
    names.push_back(checkingState ? "akira/cloud/refresh_busy"_i18n : "akira/cloud/refresh"_i18n);
    ops.push_back(Refresh);
    if (canPairState)
    {
        names.push_back("akira/cloud/pair"_i18n);
        ops.push_back(Pair);
    }
    names.push_back("akira/cloud/cache_clear"_i18n);
    ops.push_back(ClearCache);

    auto guard = alive;
    auto* dropdown = new brls::Dropdown(
        "akira/cloud/options"_i18n, names,
        [this, guard, ops](int sel) {
            if (!*guard || sel < 0 || sel >= static_cast<int>(ops.size()))
                return;
            switch (ops[static_cast<size_t>(sel)])
            {
            case Refresh: refresh(true); break;
            case Pair: openSignIn(); break;
            case ClearCache: clearCatalogCache(); break;
            }
        },
        0);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void LibraryView::clearCatalogCache()
{
    Service::instance().clearCatalogCache();
    brls::Application::notify("akira/cloud/cache_cleared"_i18n);
    refresh(true);
}

void LibraryView::updateSortButton()
{
    if (!sortButton)
        return;
    std::string key = sortState == 1 ? "akira/cloud/sort_az"
                    : sortState == 2 ? "akira/cloud/sort_za"
                                     : "akira/cloud/sort_playable";
    sortButton->setText(brls::getStr(key));
}

void LibraryView::toggleShortcut(const Game& game)
{
    if (!game.launchable())
        return;
    auto* settings = SettingsManager::getInstance();
    std::vector<Game> shortcuts = settings->getCloudShortcuts();
    auto it = std::find_if(shortcuts.begin(), shortcuts.end(),
        [&](const Game& g) { return g.productId == game.productId; });

    bool added;
    if (it != shortcuts.end())
    {
        shortcuts.erase(it);
        added = false;
    }
    else
    {
        if (game.launchLeftOut)
        {
            brls::Application::notify("akira/cloud/launch_not_built"_i18n);
            return;
        }
        if (!game.streamableNow())
        {
            showAddGameDialog(game);
            return;
        }
        shortcuts.push_back(game);
        added = true;
    }
    settings->setCloudShortcuts(shortcuts);
    settings->writeFile();
    brls::Application::notify(added ? "akira/cloud/shortcut_added"_i18n
                                    : "akira/cloud/shortcut_removed"_i18n);
}

void LibraryView::showAddGameDialog(const Game& game)
{
    std::string msg = brls::getStr("akira/cloud/add_game_body", game.name);
    /*
     * The link rather than a way to follow it: there is no browser here, so the
     * most this can do is name the page to open on something that has one.
     */
    if (!game.conceptUrl.empty())
        msg += "\n\n" + brls::getStr("akira/cloud/add_game_store", game.conceptUrl);
    akira::ui::QrCodeView::openDialog(game.conceptUrl, msg);
}

void LibraryView::openSignIn()
{
    brls::Application::pushActivity(new brls::Activity(new QrSignInView()));
}

void LibraryView::openDetail(const Game& game)
{
    brls::Application::pushActivity(new brls::Activity(new GameDetailView(game, listing.listed())));
}

} // namespace cloud
