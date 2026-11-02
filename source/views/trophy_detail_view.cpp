#include "views/trophy_detail_view.hpp"
#include "ui/theme.hpp"
#include "ui/motion.hpp"
#include "views/trophy_list_tab.hpp"

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

TrophyDetailView* TrophyDetailView::currentInstance = nullptr;
TrophySort TrophyDetailView::sortMode = TrophySort::Progress;
TrophyFilter TrophyDetailView::filterMode = TrophyFilter::All;
std::unordered_set<TrophyRowCell*> TrophyRowCell::liveCells;
std::unordered_set<std::string> TrophyRowCell::retriedIcons;

static constexpr float ROW_ICON_SIZE = 64;
static constexpr float ROW_HEIGHT = 92;
static constexpr float SUMMARY_CELL_HEIGHT = 237;


const char* trophySortLabelKey(TrophySort sort)
{
    switch (sort)
    {
        case TrophySort::Progress: return "akira/trophies/sort_progress";
        case TrophySort::DateObtained: return "akira/trophies/sort_date";
        case TrophySort::Grade: return "akira/trophies/sort_grade";
        case TrophySort::Rarity: return "akira/trophies/sort_rarity";
        case TrophySort::Name: return "akira/trophies/sort_name";
    }
    return "akira/trophies/sort_progress";
}

const char* trophyFilterLabelKey(TrophyFilter filter)
{
    switch (filter)
    {
        case TrophyFilter::All: return "akira/trophies/filter_all";
        case TrophyFilter::Earned: return "akira/trophies/filter_earned";
        case TrophyFilter::Unearned: return "akira/trophies/filter_unearned";
        case TrophyFilter::Hidden: return "akira/trophies/filter_hidden";
    }
    return "akira/trophies/filter_all";
}

bool trophyMatchesFilter(const libchiaking::trophies::Trophy& trophy, TrophyFilter filter)
{
    switch (filter)
    {
        case TrophyFilter::All: return true;
        case TrophyFilter::Earned: return trophy.earned;
        case TrophyFilter::Unearned: return !trophy.earned;
        case TrophyFilter::Hidden: return trophy.hidden;
    }
    return true;
}

static int gradeRank(libchiaking::trophies::Grade grade)
{
    switch (grade)
    {
        case libchiaking::trophies::Grade::platinum: return 0;
        case libchiaking::trophies::Grade::gold: return 1;
        case libchiaking::trophies::Grade::silver: return 2;
        case libchiaking::trophies::Grade::bronze: return 3;
    }
    return 4;
}

static uint32_t progressPercent(const libchiaking::trophies::Trophy& trophy)
{
    return trophy.progress ? trophy.progress->percent : 0;
}

void sortTrophies(std::vector<libchiaking::trophies::Trophy>& trophies, TrophySort sort)
{
    switch (sort)
    {
        case TrophySort::Progress:
            std::stable_sort(trophies.begin(), trophies.end(),
                [](const libchiaking::trophies::Trophy& a, const libchiaking::trophies::Trophy& b) {
                    bool aInProgress = !a.earned && progressPercent(a) > 0;
                    bool bInProgress = !b.earned && progressPercent(b) > 0;

                    if (aInProgress != bInProgress)
                        return aInProgress;

                    if (aInProgress && bInProgress)
                        return progressPercent(a) > progressPercent(b);

                    if (a.earned != b.earned)
                        return !a.earned;

                    return a.id < b.id;
                });
            return;

        case TrophySort::DateObtained:
            std::stable_sort(trophies.begin(), trophies.end(),
                [](const libchiaking::trophies::Trophy& a, const libchiaking::trophies::Trophy& b) {
                    bool aDated = a.earned && a.earned_at.has_value();
                    bool bDated = b.earned && b.earned_at.has_value();

                    if (aDated != bDated)
                        return aDated;

                    if (aDated && bDated)
                        return *a.earned_at > *b.earned_at;

                    if (a.earned != b.earned)
                        return a.earned;

                    return a.id < b.id;
                });
            return;

        case TrophySort::Grade:
            std::stable_sort(trophies.begin(), trophies.end(),
                [](const libchiaking::trophies::Trophy& a, const libchiaking::trophies::Trophy& b) {
                    int aRank = gradeRank(a.grade);
                    int bRank = gradeRank(b.grade);

                    if (aRank != bRank)
                        return aRank < bRank;

                    return a.id < b.id;
                });
            return;

        case TrophySort::Rarity:
            std::stable_sort(trophies.begin(), trophies.end(),
                [](const libchiaking::trophies::Trophy& a, const libchiaking::trophies::Trophy& b) {
                    const double aRate = a.earned_rate.value_or(0.0);
                    const double bRate = b.earned_rate.value_or(0.0);
                    bool aRated = aRate > 0.0;
                    bool bRated = bRate > 0.0;

                    if (aRated != bRated)
                        return aRated;

                    if (aRated && bRated && aRate != bRate)
                        return aRate < bRate;

                    if (a.rarity != b.rarity)
                        return a.rarity < b.rarity;

                    return a.id < b.id;
                });
            return;

        case TrophySort::Name:
            std::stable_sort(trophies.begin(), trophies.end(),
                [](const libchiaking::trophies::Trophy& a, const libchiaking::trophies::Trophy& b) {
                    if (a.name != b.name)
                        return a.name < b.name;

                    return a.id < b.id;
                });
            return;
    }
}

std::string formatTrophyRarity(const libchiaking::trophies::Trophy& trophy)
{
    const char* key = "akira/trophies/rarity_common";

    switch (trophy.rarity.value_or(libchiaking::trophies::Rarity::common))
    {
        case libchiaking::trophies::Rarity::ultra_rare: key = "akira/trophies/rarity_ultra_rare"; break;
        case libchiaking::trophies::Rarity::very_rare: key = "akira/trophies/rarity_very_rare"; break;
        case libchiaking::trophies::Rarity::rare: key = "akira/trophies/rarity_rare"; break;
        case libchiaking::trophies::Rarity::common: key = "akira/trophies/rarity_common"; break;
    }

    const double rate = trophy.earned_rate.value_or(0.0);
    if (rate <= 0.0)
        return brls::getStr(key);

    return std::format("{}  ·  {:.1f}%", brls::getStr(key), rate);
}

static std::string trophyTypeLabel(libchiaking::trophies::Grade grade)
{
    switch (grade)
    {
        case libchiaking::trophies::Grade::platinum: return "akira/trophies/type_platinum"_i18n;
        case libchiaking::trophies::Grade::gold: return "akira/trophies/type_gold"_i18n;
        case libchiaking::trophies::Grade::silver: return "akira/trophies/type_silver"_i18n;
        case libchiaking::trophies::Grade::bronze: return "akira/trophies/type_bronze"_i18n;
    }
    return std::string();
}

static std::string formatEarnedDate(const std::optional<libchiaking::trophies::Time>& earnedAt)
{
    if (!earnedAt)
        return std::string();

    return std::format("{:%F}", std::chrono::floor<std::chrono::days>(*earnedAt));
}

TrophyRowCell::TrophyRowCell()
{
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setCornerRadius(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setPadding(10, 14, 10, 14);
    this->setHeight(ROW_HEIGHT);
    this->setFocusable(true);

    icon = new brls::Image();
    icon->setWidth(ROW_ICON_SIZE);
    icon->setHeight(ROW_ICON_SIZE);
    icon->setScalingType(brls::ImageScalingType::FILL);
    icon->setCornerRadius(10);
    icon->setBackgroundColor(brls::Application::getTheme().getColor("color/grey_3"));
    this->addView(icon);

    auto* text = new brls::Box(brls::Axis::COLUMN);
    text->setGrow(1.0f);
    text->setMarginLeft(14);

    nameLabel = new brls::Label();
    nameLabel->setFontSize(17);
    nameLabel->setSingleLine(true);
    text->addView(nameLabel);

    detailLabel = new brls::Label();
    detailLabel->setFontSize(13);
    detailLabel->setTextColor(akira::ui::active().textDim);
    detailLabel->setSingleLine(true);
    detailLabel->setMarginTop(2);
    text->addView(detailLabel);

    progressTrack = new brls::Box(brls::Axis::ROW);
    progressTrack->setHeight(4);
    progressTrack->setCornerRadius(2);
    progressTrack->setBackgroundColor(brls::Application::getTheme().getColor("color/grey_3"));
    progressTrack->setMarginTop(7);
    progressTrack->setWidthPercentage(60);

    progressFill = new brls::Rectangle();
    progressFill->setHeight(4);
    progressFill->setCornerRadius(2);
    progressFill->setColor(akira::ui::active().accent);
    progressTrack->addView(progressFill);

    text->addView(progressTrack);
    this->addView(text);

    auto* badges = new brls::Box(brls::Axis::COLUMN);
    badges->setAlignItems(brls::AlignItems::FLEX_END);
    badges->setMarginLeft(12);

    stateLabel = new brls::Label();
    stateLabel->setFontSize(14);
    stateLabel->setSingleLine(true);
    badges->addView(stateLabel);

    rarityLabel = new brls::Label();
    rarityLabel->setFontSize(12);
    rarityLabel->setTextColor(akira::ui::active().textDim);
    rarityLabel->setSingleLine(true);
    rarityLabel->setMarginTop(4);
    badges->addView(rarityLabel);

    this->addView(badges);

    liveCells.insert(this);
}

TrophyRowCell::~TrophyRowCell()
{
    liveCells.erase(this);
}

void TrophyRowCell::bindTrophy(const libchiaking::trophies::Trophy& trophy)
{
    bool concealed = trophy.hidden && !trophy.earned;

    nameLabel->setText(concealed ? "akira/trophies/hidden_name"_i18n : trophy.name);
    detailLabel->setText(concealed ? "akira/trophies/hidden_detail"_i18n : trophy.detail);

    if (trophy.earned)
    {
        std::string date = formatEarnedDate(trophy.earned_at);
        stateLabel->setText(date.empty()
            ? std::format("{}  {}", trophyTypeLabel(trophy.grade), "akira/trophies/earned"_i18n)
            : std::format("{}  {}", trophyTypeLabel(trophy.grade), date));
        stateLabel->setTextColor(akira::ui::active().success);
    }
    else
    {
        stateLabel->setText(trophyTypeLabel(trophy.grade));
        stateLabel->setTextColor(akira::ui::active().textDim);
    }

    rarityLabel->setText(formatTrophyRarity(trophy));

    const uint64_t target = trophy.progress_target.value_or(0);
    const uint64_t reached = trophy.progress ? trophy.progress->value : 0;
    bool showProgress = trophy.progress.has_value() && target > 0 && !trophy.earned;
    progressTrack->setVisibility(showProgress ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

    if (showProgress)
    {
        float ratio = static_cast<float>(reached) / static_cast<float>(target);
        progressFill->setWidthPercentage(std::clamp(ratio, 0.0f, 1.0f) * 100.0f);

        detailLabel->setText(std::format("{}  ·  {}/{}",
            concealed ? "akira/trophies/hidden_detail"_i18n : trophy.detail,
            reached, target));
    }

    iconUrl = trophy.icon_url;
    icon->clear();

    if (iconUrl.empty())
        return;

    TrophyManager::getInstance()->fetchIcon(iconUrl,
        [](const std::string& url, const std::vector<uint8_t>& bytes) {
            bool decodeFailed = false;

            for (TrophyRowCell* cell : liveCells)
            {
                if (cell->iconUrl != url)
                    continue;

                cell->icon->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));

                if (cell->icon->getTexture() <= 0)
                    decodeFailed = true;
            }

            if (!decodeFailed)
                return;

            brls::Logger::warning("Trophy icon failed to decode ({} bytes) {}", bytes.size(), url);

            if (retriedIcons.insert(url).second)
                TrophyManager::getInstance()->discardIcon(url);
        });
}

void TrophyRowCell::prepareForReuse()
{
    iconUrl.clear();
    icon->clear();
}

void TrophyRowCell::cacheForReuse()
{
    iconUrl.clear();
    icon->clear();
}

RecyclingGridItem* TrophyRowCell::create()
{
    return new TrophyRowCell();
}

SummaryCell::SummaryCell()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setHeight(SUMMARY_CELL_HEIGHT);
    this->setCornerRadius(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setPadding(18, 24, 18, 24);

    auto* topRow = new brls::Box(brls::Axis::ROW);
    topRow->setHeight(96);
    topRow->setAlignItems(brls::AlignItems::CENTER);
    topRow->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);

    auto makeStat = [](brls::Label** outValue, const std::string& caption) {
        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setWidth(190);
        col->setAlignItems(brls::AlignItems::CENTER);
        col->setJustifyContent(brls::JustifyContent::CENTER);

        auto* value = new brls::Label();
        value->setFontSize(34);
        value->setText("0");
        col->addView(value);

        auto* cap = new brls::Label();
        cap->setFontSize(14);
        cap->setTextColor(akira::ui::active().textDim);
        cap->setText(caption);
        cap->setMarginTop(6);
        col->addView(cap);

        *outValue = value;
        return col;
    };

    topRow->addView(makeStat(&earnedValue, "akira/trophies/summary_earned"_i18n));

    ring = new ProgressRing();
    ring->setWidth(96);
    ring->setHeight(96);

    ringLabel = new brls::Label();
    ringLabel->setFontSize(22);
    ringLabel->setText("0%");
    ring->addView(ringLabel);

    topRow->addView(ring);

    topRow->addView(makeStat(&availableValue, "akira/trophies/summary_available"_i18n));

    this->addView(topRow);

    auto* separator = new brls::Rectangle();
    separator->setHeight(1);
    separator->setColor(akira::ui::withAlpha(akira::ui::active().text, 0x14));
    separator->setMarginTop(14);
    this->addView(separator);

    static const char* tierRes[4] = {
        "img/trophy/platinum.png",
        "img/trophy/gold.png",
        "img/trophy/silver.png",
        "img/trophy/bronze.png"
    };

    auto* tierRow = new brls::Box(brls::Axis::ROW);
    tierRow->setHeight(76);
    tierRow->setMarginTop(14);
    tierRow->setAlignItems(brls::AlignItems::CENTER);
    tierRow->setJustifyContent(brls::JustifyContent::SPACE_AROUND);

    for (int i = 0; i < 4; i++)
    {
        auto* col = new brls::Box(brls::Axis::COLUMN);
        col->setAlignItems(brls::AlignItems::CENTER);
        col->setJustifyContent(brls::JustifyContent::CENTER);

        auto* img = new brls::Image();
        img->setImageFromRes(tierRes[i]);
        img->setScalingType(brls::ImageScalingType::FIT);
        img->setWidth(40);
        img->setHeight(50);
        col->addView(img);

        auto* count = new brls::Label();
        count->setFontSize(16);
        count->setText("0");
        count->setMarginTop(4);
        col->addView(count);

        tierImages[i] = img;
        tierCounts[i] = count;
        tierRow->addView(col);
    }

    this->addView(tierRow);
}

void SummaryCell::bindSummary(int earned, int available, int progress, const libchiaking::trophies::TrophyCounts& counts)
{
    if (earned != prevEarned)
    {
        akira::ui::motion::countTo(earnedValue, earnedAnim,
            prevEarned < 0 ? 0 : static_cast<int>(std::lround(earnedAnim.getValue())),
            earned, 520, [](int n) { return std::to_string(n); });
        prevEarned = earned;
    }
    if (available != prevAvail)
    {
        akira::ui::motion::countTo(availableValue, availAnim,
            prevAvail < 0 ? 0 : static_cast<int>(std::lround(availAnim.getValue())),
            available, 520, [](int n) { return std::to_string(n); });
        prevAvail = available;
    }
    if (progress != prevRingPct)
    {
        akira::ui::motion::countTo(ringLabel, ringLabelAnim,
            prevRingPct < 0 ? 0 : static_cast<int>(std::lround(ringLabelAnim.getValue())),
            progress, 520, [](int n) { return std::format("{}%", n); });
        prevRingPct = progress;
    }
    ring->setProgress(progress / 100.0f);

    const int values[4] = {
        static_cast<int>(counts.platinum),
        static_cast<int>(counts.gold),
        static_cast<int>(counts.silver),
        static_cast<int>(counts.bronze)
    };

    for (int i = 0; i < 4; i++)
    {
        if (values[i] != prevTier[i])
        {
            akira::ui::motion::countTo(tierCounts[i], tierAnim[i],
                prevTier[i] < 0 ? 0 : static_cast<int>(std::lround(tierAnim[i].getValue())),
                values[i], 520, [](int n) { return std::to_string(n); });
            prevTier[i] = values[i];
        }
        tierImages[i]->setAlpha(values[i] > 0 ? 1.0f : 0.4f);
    }
}

RecyclingGridItem* SummaryCell::create()
{
    return new SummaryCell();
}

TrophyDetailSource::TrophyDetailSource(int earned, int available, int progress,
    libchiaking::trophies::TrophyCounts counts, std::vector<libchiaking::trophies::Trophy> trophies)
    : earned(earned)
    , available(available)
    , progress(progress)
    , counts(counts)
    , trophies(std::move(trophies))
{
}

size_t TrophyDetailSource::getItemCount()
{
    return trophies.size() + 1;
}

float TrophyDetailSource::heightForRow(brls::View* recycler, size_t index)
{
    return index == 0 ? SUMMARY_CELL_HEIGHT : ROW_HEIGHT;
}

RecyclingGridItem* TrophyDetailSource::cellForRow(RecyclingView* recycler, size_t index)
{
    if (index == 0)
    {
        auto* cell = dynamic_cast<SummaryCell*>(recycler->dequeueReusableCell("Summary"));
        if (!cell)
            return nullptr;

        cell->bindSummary(earned, available, progress, counts);
        return cell;
    }

    auto* cell = dynamic_cast<TrophyRowCell*>(recycler->dequeueReusableCell("TrophyRow"));
    if (!cell)
        return nullptr;

    size_t trophyIndex = index - 1;
    if (trophyIndex < trophies.size())
        cell->bindTrophy(trophies[trophyIndex]);

    return cell;
}

void TrophyDetailSource::clearData()
{
    trophies.clear();
}

TrophyDetailView::TrophyDetailView(const TrophyManager::Title& title)
    : title(title)
{
    this->inflateFromXMLRes("xml/views/trophy_detail.xml");

    currentInstance = this;

    list->isFlowMode = true;
    list->estimatedRowHeight = ROW_HEIGHT;
    list->registerCell("Summary", SummaryCell::create);
    list->registerCell("TrophyRow", TrophyRowCell::create);

    titleLabel->setText(title.name.empty() ? title.id.text() : title.name);
    subtitleLabel->setText(formatTrophyPlatforms(title.platforms));
    countsLabel->setVisibility(brls::Visibility::GONE);

    for (brls::Button* button : {groupBtn.getView(), sortBtn.getView(), filterBtn.getView()})
    {
        button->setStyle(&BUTTONSTYLE_BLUE);
        button->setBackgroundColor(akira::ui::active().surfaceElevated);
    }

    groupBtn->setVisibility(brls::Visibility::GONE);
    groupBtn->registerClickAction([this](brls::View* view) {
        showGroupPicker();
        return true;
    });

    sortBtn->registerClickAction([this](brls::View* view) {
        showSortPicker();
        return true;
    });

    filterBtn->registerClickAction([this](brls::View* view) {
        showFilterPicker();
        return true;
    });

    refreshControlLabels();

    refreshBtn->setStyle(&BUTTONSTYLE_BLUE);

    refreshGate.attach(
        refreshBtn,
        akira::ui::active().accent,
        "akira/trophies/force_refresh_btn"_i18n,
        "akira/trophies/force_refresh_busy"_i18n,
        "akira/trophies/force_refresh_wait",
        [this]() {
            if (loading)
                return psn::ActionStatus{psn::ActionState::Busy, 0};

            return TrophyManager::getInstance()->forceRefreshStatus(this->title.id.text());
        }
    );

    refreshBtn->registerClickAction([this](brls::View* view) {
        if (!refreshGate.isReady())
            return true;

        brls::Logger::info("User triggered force refresh of {}", this->title.id.text());

        TrophyManager* manager = TrophyManager::getInstance();
        brls::Application::notify("akira/trophies/force_refresh"_i18n);

        manager->recordForcedRefresh(this->title.id.text());
        load(true);
        refreshGate.apply();

        return true;
    });

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        });
}

TrophyDetailView::~TrophyDetailView()
{
    if (currentInstance == this)
        currentInstance = nullptr;
}

void TrophyDetailView::willAppear(bool resetState)
{
    Box::willAppear(resetState);

    refreshGate.start();

    if (!gate->evaluate())
    {
        loadRequested = false;
        return;
    }

    if (!loadRequested)
        load(false);
}

void TrophyDetailView::willDisappear(bool resetState)
{
    refreshGate.stop();

    Box::willDisappear(resetState);
}

void TrophyDetailView::load(bool forceRefresh)
{
    loading = true;
    loadRequested = true;

    subtitleLabel->setText("akira/trophies/loading"_i18n);

    TrophyManager::getInstance()->fetchTitleDetail(title, forceRefresh,
        [](const TrophyManager::TitleTrophies& loaded) {
            if (currentInstance && currentInstance->title.id == loaded.title)
            {
                currentInstance->loading = false;
                currentInstance->applyDetail(loaded);
            }
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

            currentInstance->subtitleLabel->setText(brls::getStr(key));
            currentInstance->list->setError(brls::getStr(key));

            brls::Logger::error("Trophy detail: load failed [{}] {}", psn::statusName(status), message);
        });
}

void TrophyDetailView::applyDetail(const TrophyManager::TitleTrophies& loaded)
{
    groups = loaded.groups;
    trophies = loaded.trophies;
    selectedGroup = 0;

    groupBtn->setVisibility(groups.size() > 1
        ? brls::Visibility::VISIBLE
        : brls::Visibility::GONE);

    applyGroup(0);
}

void TrophyDetailView::applyGroup(size_t index)
{
    if (groups.empty())
        return;

    selectedGroup = std::min(index, groups.size() - 1);
    const libchiaking::trophies::TrophyGroup& group = groups[selectedGroup];

    groupBtn->setText(group.name.empty()
        ? "akira/trophies/group_all"_i18n
        : group.name);

    subtitleLabel->setText(formatTrophyPlatforms(title.platforms));

    std::vector<libchiaking::trophies::Trophy> rows;
    rows.reserve(trophies.size());

    bool singleGroup = groups.size() <= 1;
    for (const libchiaking::trophies::Trophy& trophy : trophies)
    {
        if (!singleGroup && trophy.group != group.id)
            continue;

        if (trophyMatchesFilter(trophy, filterMode))
            rows.push_back(trophy);
    }

    sortTrophies(rows, sortMode);
    refreshControlLabels();

    list->setDataSource(new TrophyDetailSource(
        static_cast<int>(group.earned.total()), static_cast<int>(group.defined.total()),
        static_cast<int>(group.progress), group.earned, std::move(rows)));
}

void TrophyDetailView::refreshControlLabels()
{
    sortBtn->setText(brls::getStr("akira/trophies/sort_btn_value",
        brls::getStr(trophySortLabelKey(sortMode))));

    filterBtn->setText(brls::getStr("akira/trophies/filter_btn_value",
        brls::getStr(trophyFilterLabelKey(filterMode))));
}

void TrophyDetailView::showSortPicker()
{
    static const TrophySort options[] = {
        TrophySort::Progress,
        TrophySort::DateObtained,
        TrophySort::Grade,
        TrophySort::Rarity,
        TrophySort::Name
    };

    std::vector<std::string> labels;
    int selected = 0;

    for (size_t i = 0; i < std::size(options); i++)
    {
        labels.push_back(brls::getStr(trophySortLabelKey(options[i])));
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
            currentInstance->applyGroup(currentInstance->selectedGroup);
        },
        selected);

    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void TrophyDetailView::showFilterPicker()
{
    static const TrophyFilter options[] = {
        TrophyFilter::All,
        TrophyFilter::Earned,
        TrophyFilter::Unearned,
        TrophyFilter::Hidden
    };

    std::vector<std::string> labels;
    int selected = 0;

    for (size_t i = 0; i < std::size(options); i++)
    {
        labels.push_back(brls::getStr(trophyFilterLabelKey(options[i])));
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
            currentInstance->applyGroup(currentInstance->selectedGroup);
        },
        selected);

    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void TrophyDetailView::showGroupPicker()
{
    if (groups.size() <= 1)
        return;

    std::vector<std::string> labels;
    labels.reserve(groups.size());

    for (const libchiaking::trophies::TrophyGroup& group : groups)
    {
        labels.push_back(group.name.empty()
            ? group.id.text()
            : group.name);
    }

    auto* dropdown = new brls::Dropdown(
        "akira/trophies/group_picker"_i18n,
        labels,
        [](int selected) {
            if (currentInstance)
                currentInstance->applyGroup(static_cast<size_t>(selected));
        },
        static_cast<int>(selectedGroup));

    brls::Application::pushActivity(new brls::Activity(dropdown));
}
