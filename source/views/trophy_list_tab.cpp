#include "views/trophy_list_tab.hpp"
#include "ui/theme.hpp"
#include "ui/motion.hpp"
#include "views/trophy_detail_view.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

using namespace brls::literals;

static const brls::ButtonStyle BUTTONSTYLE_BLUE = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

TrophyListTab* TrophyListTab::currentInstance = nullptr;
TitleSort TrophyListTab::sortMode = TitleSort::Recent;
TitleFilter TrophyListTab::filterMode = TitleFilter::All;
std::unordered_set<TrophyGameCard*> TrophyGameCard::liveCards;
std::unordered_set<std::string> TrophyGameCard::retriedIcons;

static constexpr float CARD_COVER_HEIGHT = 206;
static constexpr float CARD_ROW_HEIGHT = 332;
static constexpr float PROFILE_HEADER_HEIGHT = 150;

static std::string formatPlayDuration(std::chrono::seconds played)
{
    const int64_t seconds = played.count();
    if (seconds <= 0)
        return std::string();

    int64_t hours = seconds / 3600;
    if (hours >= 1)
        return brls::getStr("akira/trophies/play_hours", static_cast<int>(hours));

    return brls::getStr("akira/trophies/play_minutes", static_cast<int>((seconds + 59) / 60));
}

const char* titleSortLabelKey(TitleSort sort)
{
    switch (sort)
    {
        case TitleSort::Recent: return "akira/trophies/tsort_recent";
        case TitleSort::Progress: return "akira/trophies/tsort_progress";
        case TitleSort::Name: return "akira/trophies/tsort_name";
        case TitleSort::Earned: return "akira/trophies/tsort_earned";
    }
    return "akira/trophies/tsort_recent";
}

const char* titleFilterLabelKey(TitleFilter filter)
{
    switch (filter)
    {
        case TitleFilter::All: return "akira/trophies/tfilter_all";
        case TitleFilter::InProgress: return "akira/trophies/tfilter_in_progress";
        case TitleFilter::Completed: return "akira/trophies/tfilter_completed";
        case TitleFilter::Unstarted: return "akira/trophies/tfilter_unstarted";
        case TitleFilter::Hidden: return "akira/trophies/tfilter_hidden";
    }
    return "akira/trophies/tfilter_all";
}

static bool titleMatchesFilter(const TrophyManager::Title& title, TitleFilter filter)
{
    const uint64_t earned = title.earned.total();
    const uint64_t defined = title.defined.total();

    switch (filter)
    {
        case TitleFilter::All: return !title.hidden;
        case TitleFilter::InProgress: return !title.hidden && earned > 0 && earned < defined;
        case TitleFilter::Completed: return !title.hidden && defined > 0 && earned >= defined;
        case TitleFilter::Unstarted: return !title.hidden && earned == 0;
        case TitleFilter::Hidden: return title.hidden;
    }
    return true;
}

static bool updatedLater(const TrophyManager::Title& a, const TrophyManager::Title& b)
{
    return a.last_updated > b.last_updated;
}

static std::string formatLastPlayed(int64_t playedAt)
{
    int64_t age = static_cast<int64_t>(std::time(nullptr)) - playedAt;

    if (age < 90)
        return brls::getStr("akira/trophies/played_just_now");

    if (age < 3600)
        return brls::getStr("akira/trophies/played_minutes_ago", static_cast<int>(age / 60));

    if (age < 86400)
        return brls::getStr("akira/trophies/played_hours_ago", static_cast<int>(age / 3600));

    return brls::getStr("akira/trophies/played_days_ago", static_cast<int>(age / 86400));
}

std::string formatRelativeAge(int64_t savedAt)
{
    if (savedAt <= 0)
        return std::string();

    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t age = now - savedAt;

    if (age < 0)
        return brls::getStr("akira/trophies/updated_just_now");

    if (age < 90)
        return brls::getStr("akira/trophies/updated_just_now");

    if (age < 3600)
        return brls::getStr("akira/trophies/updated_minutes", static_cast<int>(age / 60));

    if (age < 86400)
        return brls::getStr("akira/trophies/updated_hours", static_cast<int>(age / 3600));

    return brls::getStr("akira/trophies/updated_days", static_cast<int>(age / 86400));
}

std::string formatTrophyPlatforms(const std::vector<libchiaking::trophies::TrophyPlatform>& platforms)
{
    std::string out;

    for (const libchiaking::trophies::TrophyPlatform platform : platforms)
    {
        if (!out.empty())
            out += ", ";

        switch (platform)
        {
            case libchiaking::trophies::TrophyPlatform::ps3: out += "PS3"; break;
            case libchiaking::trophies::TrophyPlatform::ps4: out += "PS4"; break;
            case libchiaking::trophies::TrophyPlatform::ps5: out += "PS5"; break;
            case libchiaking::trophies::TrophyPlatform::ps_vita: out += "PS Vita"; break;
            case libchiaking::trophies::TrophyPlatform::pc: out += "PC"; break;
        }
    }

    return out;
}

std::string formatTrophyCounts(const libchiaking::trophies::TrophyCounts& counts)
{
    std::string out;

    auto append = [&out](uint32_t value, const char* key) {
        if (value <= 0)
            return;
        if (!out.empty())
            out += ", ";
        out += brls::getStr(key, value);
    };

    append(counts.platinum, "akira/trophies/count_platinum");
    append(counts.gold, "akira/trophies/count_gold");
    append(counts.silver, "akira/trophies/count_silver");
    append(counts.bronze, "akira/trophies/count_bronze");

    return out;
}

TrophyGameCard::TrophyGameCard()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setCornerRadius(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setPadding(12, 12, 12, 12);
    this->setFocusable(true);

    cover = new brls::Image();
    cover->setHeight(CARD_COVER_HEIGHT);
    cover->setScalingType(brls::ImageScalingType::FILL);
    cover->setCornerRadius(10);
    cover->setBackgroundColor(brls::Application::getTheme().getColor("color/grey_3"));
    this->addView(cover);

    nameLabel = new brls::Label();
    nameLabel->setFontSize(22);
    nameLabel->setSingleLine(true);
    nameLabel->setAutoAnimate(true);
    nameLabel->setMarginTop(10);
    this->addView(nameLabel);

    playLabel = new brls::Label();
    playLabel->setFontSize(17);
    playLabel->setTextColor(akira::ui::active().accent);
    playLabel->setSingleLine(true);
    playLabel->setMarginTop(5);
    playLabel->setVisibility(brls::Visibility::GONE);
    this->addView(playLabel);

    detailLabel = new brls::Label();
    detailLabel->setFontSize(17);
    detailLabel->setTextColor(akira::ui::active().textDim);
    detailLabel->setSingleLine(true);
    detailLabel->setMarginTop(3);
    this->addView(detailLabel);

    progressTrack = new brls::Box(brls::Axis::ROW);
    progressTrack->setHeight(4);
    progressTrack->setCornerRadius(2);
    progressTrack->setBackgroundColor(brls::Application::getTheme().getColor("color/grey_3"));
    progressTrack->setMarginTop(7);

    progressFill = new brls::Rectangle();
    progressFill->setHeight(4);
    progressFill->setCornerRadius(2);
    progressFill->setColor(akira::ui::active().accent);
    progressTrack->addView(progressFill);

    this->addView(progressTrack);

    akira::ui::motion::liftOnFocus(this, focusAnim);

    this->registerClickAction([this](brls::View*) {
        if (selectCallback)
            selectCallback();
        return true;
    });

    liveCards.insert(this);
}

TrophyGameCard::~TrophyGameCard()
{
    liveCards.erase(this);
}

void TrophyGameCard::bind(const TrophyManager::Title& title, std::function<void()> onSelect)
{
    selectCallback = std::move(onSelect);
    this->setVisibility(brls::Visibility::VISIBLE);
    this->setFocusable(true);

    nameLabel->setText(title.name.empty() ? title.id.text() : title.name);

    detailLabel->setText(std::format("{}  ·  {}%  ·  {}/{}",
        formatTrophyPlatforms(title.platforms),
        title.progress,
        title.earned.total(),
        title.defined.total()));

    std::optional<TrophyManager::PlayedGame> play = TrophyManager::getInstance()->playedGameFor(title.id);

    if (play && play->play_time && play->play_time->count() > 0)
    {
        std::string played = formatPlayDuration(*play->play_time);

        if (play->last_played)
        {
            std::string last = formatLastPlayed(std::chrono::duration_cast<std::chrono::seconds>(
                play->last_played->time_since_epoch()).count());
            playLabel->setText(brls::getStr("akira/trophies/play_and_last", played, last));
        }
        else
        {
            playLabel->setText(played);
        }

        playLabel->setVisibility(brls::Visibility::VISIBLE);
    }
    else
    {
        playLabel->setVisibility(brls::Visibility::GONE);
    }

    float ratio = std::clamp(static_cast<float>(title.progress) / 100.0f, 0.0f, 1.0f);
    progressFill->setWidthPercentage(ratio * 100.0f);

    iconUrl = title.icon_url;
    cover->clear();

    if (iconUrl.empty())
        return;

    TrophyManager::getInstance()->fetchIcon(iconUrl,
        [](const std::string& url, const std::vector<uint8_t>& bytes) {
            bool decodeFailed = false;

            for (TrophyGameCard* card : liveCards)
            {
                if (card->iconUrl != url)
                    continue;

                card->cover->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));

                if (card->cover->getTexture() <= 0)
                    decodeFailed = true;
            }

            if (!decodeFailed)
                return;

            brls::Logger::warning("Trophy cover failed to decode ({} bytes) {}", bytes.size(), url);

            if (retriedIcons.insert(url).second)
                TrophyManager::getInstance()->discardIcon(url);
        });
}

void TrophyGameCard::reset()
{
    selectCallback = nullptr;
    iconUrl.clear();
    cover->clear();
}

TrophyGameRowCell::TrophyGameRowCell()
{
    this->setAxis(brls::Axis::ROW);
    this->setWidthPercentage(100.0f);
    this->setFocusable(false);

    for (int i = 0; i < 3; i++)
    {
        cards[i] = new TrophyGameCard();
        cards[i]->setGrow(1.0f);
        if (i > 0)
            cards[i]->setMarginLeft(18);
        this->addView(cards[i]);
    }
}

void TrophyGameRowCell::bindRow(const std::vector<TrophyManager::Title>& rowTitles, size_t startIndex)
{
    for (int i = 0; i < 3; i++)
    {
        size_t idx = startIndex + static_cast<size_t>(i);
        if (idx < rowTitles.size())
        {
            const TrophyManager::Title title = rowTitles[idx];
            cards[i]->bind(title, [title]() {
                brls::Application::pushActivity(new brls::Activity(new TrophyDetailView(title)));
            });
        }
        else
        {
            cards[i]->reset();
            cards[i]->setFocusable(false);
            cards[i]->setVisibility(brls::Visibility::INVISIBLE);
        }
    }
}

void TrophyGameRowCell::prepareForReuse()
{
    for (int i = 0; i < 3; i++)
        if (cards[i])
            cards[i]->reset();
}

RecyclingGridItem* TrophyGameRowCell::create()
{
    return new TrophyGameRowCell();
}

TrophyProfileCell::TrophyProfileCell()
{
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setHeight(PROFILE_HEADER_HEIGHT);
    this->setCornerRadius(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setPadding(18, 20, 18, 20);
    this->setFocusable(true);

    levelRing = new ProgressRing();
    levelRing->setWidth(104);
    levelRing->setHeight(104);
    levelRing->setThickness(8);
    levelRing->setMarginRight(22);

    levelValue = new brls::Label();
    levelValue->setText("-");
    levelValue->setFontSize(34);
    levelValue->setSingleLine(true);
    levelRing->addView(levelValue);

    this->addView(levelRing);

    static const char* tierRes[4] = {
        "img/trophy/platinum.png",
        "img/trophy/gold.png",
        "img/trophy/silver.png",
        "img/trophy/bronze.png"
    };
    static const char* tierLabel[4] = {
        "akira/trophies/type_platinum",
        "akira/trophies/type_gold",
        "akira/trophies/type_silver",
        "akira/trophies/type_bronze"
    };

    auto* tilesRow = new brls::Box(brls::Axis::ROW);
    tilesRow->setGrow(1.0f);

    for (int i = 0; i < 4; i++)
    {
        auto* tile = new brls::Box(brls::Axis::ROW);
        tile->setGrow(1.0f);
        if (i > 0)
            tile->setMarginLeft(12);
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setCornerRadius(10);
        tile->setBackgroundColor(akira::ui::active().surfaceElevated);
        tile->setPadding(12, 12, 12, 12);

        auto* img = new brls::Image();
        img->setImageFromRes(tierRes[i]);
        img->setScalingType(brls::ImageScalingType::FIT);
        img->setWidth(28);
        img->setHeight(34);
        img->setMarginRight(11);
        tile->addView(img);

        auto* textCol = new brls::Box(brls::Axis::COLUMN);

        auto* count = new brls::Label();
        count->setFontSize(23);
        count->setText("0");
        textCol->addView(count);

        auto* label = new brls::Label();
        label->setText(brls::getStr(tierLabel[i]));
        label->setFontSize(13);
        label->setTextColor(akira::ui::active().textDim);
        label->setMarginTop(2);
        textCol->addView(label);

        tile->addView(textCol);

        tierImages[i] = img;
        tierCounts[i] = count;
        tilesRow->addView(tile);
    }

    this->addView(tilesRow);
}

void TrophyProfileCell::bindSummary(const TrophyManager::Summary& summary)
{
    levelValue->setText(std::to_string(summary.level));

    if (levelRing)
        levelRing->setProgress(static_cast<float>(std::min<uint32_t>(summary.progress, 100)) / 100.0f);

    const uint32_t values[4] = {
        summary.earned.platinum,
        summary.earned.gold,
        summary.earned.silver,
        summary.earned.bronze
    };

    for (int i = 0; i < 4; i++)
    {
        tierCounts[i]->setText(std::to_string(values[i]));
        tierImages[i]->setAlpha(values[i] > 0 ? 1.0f : 0.4f);
    }
}

RecyclingGridItem* TrophyProfileCell::create()
{
    return new TrophyProfileCell();
}

TrophyGridDataSource::TrophyGridDataSource(std::vector<TrophyManager::Title> titles, TrophyManager::Summary summary, bool haveSummary)
    : titles(std::move(titles)), summary(std::move(summary)), haveSummary(haveSummary)
{
}

size_t TrophyGridDataSource::getItemCount()
{
    return 1 + (titles.size() + 2) / 3;
}

float TrophyGridDataSource::heightForRow(brls::View* recycler, size_t index)
{
    return index == 0 ? PROFILE_HEADER_HEIGHT : CARD_ROW_HEIGHT;
}

RecyclingGridItem* TrophyGridDataSource::cellForRow(RecyclingView* recycler, size_t index)
{
    if (index == 0)
    {
        auto* cell = dynamic_cast<TrophyProfileCell*>(recycler->dequeueReusableCell("ProfileHeader"));
        if (!cell)
            return nullptr;

        if (haveSummary)
            cell->bindSummary(summary);

        return cell;
    }

    auto* cell = dynamic_cast<TrophyGameRowCell*>(recycler->dequeueReusableCell("TrophyRow"));
    if (!cell)
        return nullptr;

    cell->bindRow(titles, (index - 1) * 3);

    return cell;
}

void TrophyGridDataSource::onItemSelected(brls::Box* recycler, size_t index)
{
}

void TrophyGridDataSource::clearData()
{
    titles.clear();
}

TrophyListTab::TrophyListTab()
{
    this->inflateFromXMLRes("xml/tabs/trophies.xml");

    currentInstance = this;

    grid->registerCell("ProfileHeader", TrophyProfileCell::create);
    grid->registerCell("TrophyRow", TrophyGameRowCell::create);
    grid->isFlowMode = true;
    grid->estimatedRowHeight = CARD_ROW_HEIGHT;

    for (brls::Button* button : {sortBtn.getView(), filterBtn.getView()})
        button->setStyle(&BUTTONSTYLE_BLUE);

    sortBtn->setBackgroundColor(akira::ui::active().surfaceElevated);
    filterBtn->setBackgroundColor(akira::ui::active().surfaceElevated);

    forceRefreshBtn->setStyle(&BUTTONSTYLE_BLUE);
    forceRefreshBtn->setBackgroundColor(akira::ui::active().surfaceElevated);
    forceRefreshBtn->setShrink(0.0f);
    forceRefreshBtn->setCornerRadius(8);

    sortBtn->registerClickAction([this](brls::View* view) {
        showSortPicker();
        return true;
    });

    filterBtn->registerClickAction([this](brls::View* view) {
        showFilterPicker();
        return true;
    });

    refreshControlLabels();

    forceRefreshGate.attach(
        forceRefreshBtn,
        akira::ui::active().surfaceElevated,
        "akira/trophies/refresh"_i18n,
        "akira/trophies/force_refresh_busy"_i18n,
        "akira/trophies/force_refresh_wait",
        [this]() {
            if (loading)
                return psn::ActionStatus{psn::ActionState::Busy, 0};

            return TrophyManager::getInstance()->forceRefreshStatus();
        }
    );

    forceRefreshBtn->registerClickAction([this](brls::View* view) {
        if (!forceRefreshGate.isReady())
            return true;

        brls::Logger::info("User triggered force refresh");

        TrophyManager* manager = TrophyManager::getInstance();
        brls::Application::notify("akira/trophies/force_refresh"_i18n);

        manager->recordForcedRefresh();
        load(true);
        forceRefreshGate.apply();

        return true;
    });

    TrophyManager* trophies = TrophyManager::getInstance();

    trophies->setSummaryObserver([](const TrophyManager::Summary& summary) {
        if (currentInstance)
            currentInstance->applySummary(summary);
    });

    trophies->setLibraryObserver([](const std::vector<TrophyManager::Title>& titles) {
        if (currentInstance)
            currentInstance->applyTitles(titles);
    });

    trophies->startAutoRefresh();
}

TrophyListTab::~TrophyListTab()
{
    if (currentInstance == this)
        currentInstance = nullptr;
}

brls::View* TrophyListTab::create()
{
    return new TrophyListTab();
}

void TrophyListTab::willAppear(bool resetState)
{
    Box::willAppear(resetState);

    forceRefreshGate.start();
    statusTimer.setCallback([this]() { refreshStatusLine(); });
    statusTimer.start(30 * 1000);
    refreshStatusLine();

    if (!gate->evaluate())
    {
        loadRequested = false;
        return;
    }

    if (!loadRequested)
        load(false);
}

void TrophyListTab::willDisappear(bool resetState)
{
    forceRefreshGate.stop();
    statusTimer.stop();

    Box::willDisappear(resetState);
}

void TrophyListTab::applySummary(const TrophyManager::Summary& summary)
{
    currentSummary = summary;
    haveSummary = true;

    if (currentDataSource)
    {
        currentDataSource->summary = summary;
        currentDataSource->haveSummary = true;
        grid->notifyDataChanged();
    }
}

void TrophyListTab::applyTitles(const std::vector<TrophyManager::Title>& titles)
{
    this->titles = titles;
    rebuildGrid();

    TrophyManager::getInstance()->resolvePlayedGames(titles, []() {
        if (currentInstance)
            currentInstance->rebuildGrid();
    });
}

void TrophyListTab::rebuildGrid()
{
    std::vector<TrophyManager::Title> rows;
    rows.reserve(titles.size());

    for (const TrophyManager::Title& title : titles)
    {
        if (titleMatchesFilter(title, filterMode))
            rows.push_back(title);
    }

    switch (sortMode)
    {
        case TitleSort::Recent:
            std::stable_sort(rows.begin(), rows.end(), updatedLater);
            break;

        case TitleSort::Progress:
            std::stable_sort(rows.begin(), rows.end(),
                [](const TrophyManager::Title& a, const TrophyManager::Title& b) {
                    if (a.progress != b.progress)
                        return a.progress > b.progress;
                    return updatedLater(a, b);
                });
            break;

        case TitleSort::Name:
            std::stable_sort(rows.begin(), rows.end(),
                [](const TrophyManager::Title& a, const TrophyManager::Title& b) {
                    return a.name < b.name;
                });
            break;

        case TitleSort::Earned:
            std::stable_sort(rows.begin(), rows.end(),
                [](const TrophyManager::Title& a, const TrophyManager::Title& b) {
                    const uint64_t aEarned = a.earned.total();
                    const uint64_t bEarned = b.earned.total();

                    if (aEarned != bEarned)
                        return aEarned > bEarned;

                    return updatedLater(a, b);
                });
            break;
    }

    refreshControlLabels();
    refreshStatusLine();

    if (rows.empty())
    {
        currentDataSource = nullptr;
        grid->setEmpty(filterMode == TitleFilter::All
            ? "akira/trophies/empty"_i18n
            : "akira/trophies/empty_filter"_i18n);
        return;
    }

    auto* dataSource = new TrophyGridDataSource(std::move(rows), currentSummary, haveSummary);
    currentDataSource = dataSource;
    grid->setDataSource(dataSource);
}

void TrophyListTab::refreshControlLabels()
{
    sortBtn->setText(brls::getStr("akira/trophies/sort_btn_value",
        brls::getStr(titleSortLabelKey(sortMode))));

    filterBtn->setText(brls::getStr("akira/trophies/filter_btn_value",
        brls::getStr(titleFilterLabelKey(filterMode))));
}

void TrophyListTab::refreshStatusLine()
{
    statusLabel->setText(formatRelativeAge(TrophyManager::getInstance()->librarySavedAtSeconds()));
    statusLabel->setTextColor(akira::ui::active().textDim);
}

void TrophyListTab::showSortPicker()
{
    static const TitleSort options[] = {
        TitleSort::Recent,
        TitleSort::Progress,
        TitleSort::Name,
        TitleSort::Earned
    };

    std::vector<std::string> labels;
    int selected = 0;

    for (size_t i = 0; i < std::size(options); i++)
    {
        labels.push_back(brls::getStr(titleSortLabelKey(options[i])));
        if (options[i] == sortMode)
            selected = static_cast<int>(i);
    }

    auto* dropdown = new brls::Dropdown(
        "akira/trophies/sort_picker"_i18n,
        labels,
        [](int chosen) {
            if (!currentInstance || chosen < 0 || chosen >= static_cast<int>(std::size(options)))
                return;

            sortMode = options[chosen];
            currentInstance->rebuildGrid();
        },
        selected);

    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void TrophyListTab::showFilterPicker()
{
    static const TitleFilter options[] = {
        TitleFilter::All,
        TitleFilter::InProgress,
        TitleFilter::Completed,
        TitleFilter::Unstarted,
        TitleFilter::Hidden
    };

    std::vector<std::string> labels;
    int selected = 0;

    for (size_t i = 0; i < std::size(options); i++)
    {
        labels.push_back(brls::getStr(titleFilterLabelKey(options[i])));
        if (options[i] == filterMode)
            selected = static_cast<int>(i);
    }

    auto* dropdown = new brls::Dropdown(
        "akira/trophies/filter_picker"_i18n,
        labels,
        [](int chosen) {
            if (!currentInstance || chosen < 0 || chosen >= static_cast<int>(std::size(options)))
                return;

            filterMode = options[chosen];
            currentInstance->rebuildGrid();
        },
        selected);

    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void TrophyListTab::load(bool forceRefresh)
{
    if (loading)
    {
        brls::Logger::info("Trophy grid: load already in progress, ignoring request");
        return;
    }

    loading = true;
    loadRequested = true;

    if (titles.empty())
        grid->showSkeleton(12);

    TrophyManager* trophies = TrophyManager::getInstance();

    trophies->fetchSummary(forceRefresh,
        [](const TrophyManager::Summary& summary) {
            if (currentInstance)
                currentInstance->applySummary(summary);
        },
        [](psn::Status status, const std::string& message) {
            brls::Logger::warning("Trophy grid: summary unavailable [{}] {}",
                psn::statusName(status), message);
            if (currentInstance)
                currentInstance->statusLabel->setText("akira/trophies/summary_unavailable"_i18n);
        });

    trophies->fetchLibrary(forceRefresh,
        [](const std::vector<TrophyManager::Title>& titles) {
            if (!currentInstance)
                return;

            currentInstance->loading = false;
            currentInstance->applyTitles(titles);
        },
        [](psn::Status status, const std::string& message) {
            if (!currentInstance)
                return;

            currentInstance->loading = false;

            const char* key = "akira/trophies/error_generic";
            switch (status)
            {
                case psn::Status::NotLinked: key = "akira/trophies/error_not_linked"; break;
                case psn::Status::SessionExpired: key = "akira/trophies/error_session_expired"; break;
                case psn::Status::Offline: key = "akira/trophies/error_offline"; break;
                case psn::Status::RateLimited: key = "akira/trophies/error_rate_limited"; break;
                default: break;
            }

            brls::Logger::error("Trophy grid: load failed [{}] {}", psn::statusName(status), message);

            if (currentInstance->titles.empty())
                currentInstance->grid->setError(brls::getStr(key));
            else
                brls::Application::notify(brls::getStr(key));
        });
}
