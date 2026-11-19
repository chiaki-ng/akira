#include "cloud/views/library_view.hpp"
#include "app/view_services.hpp"
#include "log/log.hpp"

#include "cloud/cloud_shortcuts.hpp"
#include "cloud/views/cloud_catalog_data_source.hpp"
#include "cloud/views/cloud_game_row_cell.hpp"
#include "cloud/views/game_detail_view.hpp"

#include "app/view_services.hpp"
#include "trophies/trophy_manager.hpp"
#include "app/views/motion.hpp"
#include "app/views/qr_code_view.hpp"
#include "app/views/theme.hpp"
#include "app/views/shared_view_holder.hpp"
#include "accounts/views/qr_sign_in_view.hpp"
#include "play/views/stream_view.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace akira::cloud::views {

using namespace brls::literals;

LibraryView::LibraryView()
{
    if (auto saved = LibraryListing::filterNamed(app::ViewServices::current().cloudSettings.filter()))
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

    sortState = app::ViewServices::current().cloudSettings.sortState();

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

    grid = new app::views::RecyclingGrid();
    grid->setGrow(1.0f);
    grid->registerCell("CloudRow", CloudGameRowCell::create);
    grid->estimatedRowHeight = CloudGameRowCell::Height;
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

    log::info("CloudLib: refresh(force={}) gen={}", force, gen);
    fetch.follow(gen);
    renderSnapshot(app::ViewServices::current().cloud.snapshotForActiveProfile());
    app::ViewServices::current().cloud.refreshActiveProfile(force,
        [this, guard, gen](const Snapshot& snapshot) {
            log::info("CloudLib: refresh cb gen={} cur={} alive={} avail={} hasCatalog={} games={}",
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
        app::views::motion::crossfade(loadingLines->box, stepFade, [this]() { writeLoadingLines(); });
    else if (!stepFade.isRunning())
        writeLoadingLines();
}

void LibraryView::addLoadingLines(brls::Box* panel, const std::string& waitingText)
{
    const auto& pal = app::views::active();

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
    const auto& pal = app::views::active();
    const Status& status = snapshot.status;

    libraryStatus = LibraryStatus(status);

    std::string chipText;
    NVGcolor chipColor = pal.textDim;
    const auto chip = libraryStatus.chip();
    const bool chipShown = chip.has_value();
    if (chip)
    {
        switch (*chip)
        {
            case LibraryStatus::Chip::Ready:
                chipText = "akira/cloud/chip_ready"_i18n;
                chipColor = pal.success;
                break;
            case LibraryStatus::Chip::Catalog:
                chipText = "akira/cloud/chip_catalog"_i18n;
                chipColor = pal.warning;
                break;
            case LibraryStatus::Chip::Unavailable:
                chipText = "akira/cloud/chip_unavailable"_i18n;
                chipColor = pal.danger;
                break;
            case LibraryStatus::Chip::Warning:
                chipText = "akira/cloud/chip_warning"_i18n;
                chipColor = pal.warning;
                break;
            case LibraryStatus::Chip::Empty:
                chipText = "akira/cloud/chip_empty"_i18n;
                chipColor = pal.textDim;
                break;
            case LibraryStatus::Chip::Pair:
                chipText = "akira/cloud/chip_pair"_i18n;
                chipColor = pal.accent;
                break;
            case LibraryStatus::Chip::Checking:
                chipText = "akira/cloud/chip_checking"_i18n;
                chipColor = pal.accent;
                break;
            case LibraryStatus::Chip::Error:
                chipText = "akira/cloud/chip_error"_i18n;
                chipColor = pal.danger;
                break;
        }
    }

    statusChip->setVisibility(chipShown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (chipShown)
    {
        statusChipLabel->setText(chipText);
        statusChipLabel->setTextColor(chipColor);
        statusChip->setBackgroundColor(app::views::withAlpha(chipColor, 0x2e));
    }

    (void)pal;

    if (!snapshot.hasCatalog || snapshot.catalog.games.empty())
    {
        log::info("CloudLib: renderSnapshot -> STATE (avail={} hasCatalog={} games={})",
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

    log::info("CloudLib: renderSnapshot -> CATALOG ({} games)", snapshot.catalog.games.size());

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
    const auto& pal = app::views::active();
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

    const LibraryStatus shown(st);
    std::string glyph = "\xEE\x8A\xBD";
    switch (shown.glyph())
    {
        case LibraryStatus::Glyph::Pair: glyph = "\xEE\x85\x97"; break;
        case LibraryStatus::Glyph::Alert: glyph = "\xEE\x8B\x81"; break;
        case LibraryStatus::Glyph::Cloud: break;
    }
    NVGcolor glyphColor = pal.textDim;
    switch (shown.tone())
    {
        case LibraryStatus::Tone::Accent: glyphColor = pal.accent; break;
        case LibraryStatus::Tone::Danger: glyphColor = pal.danger; break;
        case LibraryStatus::Tone::Dim: break;
    }
    std::string title = st.title;
    std::string detail = st.detail;
    if (shown.emptyLibrary())
    {
        title = "akira/cloud/empty_none_title"_i18n;
        detail = "akira/cloud/empty_none_detail"_i18n;
    }
    const LibraryStatus::Action action = shown.action();

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

    if (shown.checking())
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
    if (action == LibraryStatus::Action::Pair)
    {
        actionBtn = new brls::Button();
        actionBtn->setText("akira/cloud/pair"_i18n);
        actionBtn->registerClickAction([this](brls::View*) { openSignIn(); return true; });
        panel->addView(actionBtn);
    }
    else if (action == LibraryStatus::Action::Refresh)
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
    listing.setNow(app::ViewServices::current().cloud.trustedNowMs().value_or(systemNow));
    listing.setGames(snapshot.catalog.games);
    listing.setRecentlyPlayed(snapshot.recentlyPlayed);
    listing.settle(true);
    scheduleUnlock();
    applyFilter();
}

void LibraryView::scheduleUnlock()
{
    const uint64_t scheduled = ++unlockGeneration;
    const auto wait = listing.unlockDelay();
    if (!wait)
        return;
    auto guard = alive;
    brls::delay(static_cast<long>(*wait), [this, guard, scheduled]() {
        if (!*guard || scheduled != unlockGeneration)
            return;
        log::info("CloudLib: a scheduled release is due, reloading the library");
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
    const auto& pal = app::views::active();
    chip->setStyle(&brls::BUTTONSTYLE_BORDERLESS);
    chip->setCornerRadius(16);
    chip->setHeight(36);
    chip->setPaddingLeft(14);
    chip->setPaddingRight(14);
    chip->setFontSize(16);
    chip->setTextColor(selected ? pal.accent : pal.textMuted);
    chip->setBackgroundColor(selected ? app::views::withAlpha(pal.accent, 0x40) : pal.surface);
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
    auto* services = &app::ViewServices::current();
    services->cloudSettings.setFilter(std::string(LibraryListing::key(filter)));
    services->store.save();
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
    std::vector<Game> filtered = listing.ordered(LibraryListing::orderOf(sortState));

    rebuildFilterControls();

    const std::size_t listed = listing.listed().size();
    if (countLabel)
    {
        countLabel->setVisibility(listed == 0 ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        countLabel->setTextColor(app::views::active().textDim);
        countLabel->setText(filtered.size() != listed
            ? brls::getStr("akira/cloud/count_filtered", filtered.size(), listed)
            : brls::getStr("akira/cloud/count_all", listed));
    }

    log::info("CloudLib: applyFilter tab={} platform={} q='{}' -> {}/{} games",
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
            app::ViewServices::current().cloudSettings.setSortState(sortState);
            app::ViewServices::current().store.save();
            updateSortButton();
            applyFilter();
        },
        sortState);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void LibraryView::openOverflowMenu()
{
    const std::vector<LibraryStatus::Option> ops = libraryStatus.options();
    std::vector<std::string> names;
    for (LibraryStatus::Option op : ops)
    {
        switch (op)
        {
            case LibraryStatus::Option::Refresh:
                names.push_back(libraryStatus.checking() ? "akira/cloud/refresh_busy"_i18n : "akira/cloud/refresh"_i18n);
                break;
            case LibraryStatus::Option::Pair:
                names.push_back("akira/cloud/pair"_i18n);
                break;
            case LibraryStatus::Option::ClearCache:
                names.push_back("akira/cloud/cache_clear"_i18n);
                break;
        }
    }

    auto guard = alive;
    auto* dropdown = new brls::Dropdown(
        "akira/cloud/options"_i18n, names,
        [this, guard, ops](int sel) {
            if (!*guard || sel < 0 || sel >= static_cast<int>(ops.size()))
                return;
            switch (ops[static_cast<size_t>(sel)])
            {
            case LibraryStatus::Option::Refresh: refresh(true); break;
            case LibraryStatus::Option::Pair: openSignIn(); break;
            case LibraryStatus::Option::ClearCache: clearCatalogCache(); break;
            }
        },
        0);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void LibraryView::clearCatalogCache()
{
    app::ViewServices::current().cloud.clearCatalogCache();
    brls::Application::notify("akira/cloud/cache_cleared"_i18n);
    refresh(true);
}

void LibraryView::updateSortButton()
{
    if (!sortButton)
        return;
    std::string key;
    switch (LibraryListing::orderOf(sortState))
    {
        case LibraryOrder::AToZ: key = "akira/cloud/sort_az"; break;
        case LibraryOrder::ZToA: key = "akira/cloud/sort_za"; break;
        case LibraryOrder::Playable: key = "akira/cloud/sort_playable"; break;
    }
    sortButton->setText(brls::getStr(key));
}

void LibraryView::toggleShortcut(const Game& game)
{
    auto* services = &app::ViewServices::current();
    CloudShortcuts shortcuts(services->accounts.cloudShortcuts());

    bool added = false;
    switch (shortcuts.toggle(game))
    {
        case CloudShortcuts::Toggle::Ignored:
            return;
        case CloudShortcuts::Toggle::NotBuilt:
            brls::Application::notify("akira/cloud/launch_not_built"_i18n);
            return;
        case CloudShortcuts::Toggle::NeedsLibrary:
            showAddGameDialog(game);
            return;
        case CloudShortcuts::Toggle::Removed:
            added = false;
            break;
        case CloudShortcuts::Toggle::Added:
            added = true;
            break;
    }
    services->accounts.setCloudShortcuts(shortcuts.games());
    services->store.save();
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
    app::views::QrCodeView::openDialog(game.conceptUrl, msg);
}

void LibraryView::openSignIn()
{
    brls::Application::pushActivity(new brls::Activity(new accounts::views::QrSignInView()));
}

void LibraryView::openDetail(const Game& game)
{
    brls::Application::pushActivity(new brls::Activity(new GameDetailView(game, listing.listed())));
}

} // namespace akira::cloud::views
