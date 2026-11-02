#ifndef AKIRA_TROPHY_LIST_TAB_HPP
#define AKIRA_TROPHY_LIST_TAB_HPP

#include <borealis.hpp>

#include <string>
#include <unordered_set>
#include <vector>

#include "core/trophy_manager.hpp"
#include "views/psn_action_button.hpp"
#include "views/psn_gated_box.hpp"
#include "views/progress_ring.hpp"
#include "views/vendored/switchfin/recycling_grid.hpp"

#include <functional>

class TrophyGameCard : public brls::Box {
public:
    TrophyGameCard();
    ~TrophyGameCard() override;

    void bind(const TrophyManager::Title& title, std::function<void()> onSelect);
    void reset();

    static std::unordered_set<TrophyGameCard*> liveCards;
    static std::unordered_set<std::string> retriedIcons;

private:
    brls::Image* cover = nullptr;
    brls::Label* nameLabel = nullptr;
    brls::Label* playLabel = nullptr;
    brls::Label* detailLabel = nullptr;
    brls::Box* progressTrack = nullptr;
    brls::Rectangle* progressFill = nullptr;

    brls::Animatable focusAnim{0.0f};

    std::string iconUrl;
    std::function<void()> selectCallback;
};

class TrophyGameRowCell : public RecyclingGridItem {
public:
    TrophyGameRowCell();

    void bindRow(const std::vector<TrophyManager::Title>& titles, size_t startIndex);
    void prepareForReuse() override;

    static RecyclingGridItem* create();

private:
    TrophyGameCard* cards[3] = {};
};

class TrophyProfileCell : public RecyclingGridItem {
public:
    TrophyProfileCell();

    void bindSummary(const TrophyManager::Summary& summary);

    static RecyclingGridItem* create();

private:
    brls::Label* levelValue = nullptr;
    ProgressRing* levelRing = nullptr;
    brls::Image* tierImages[4] = {};
    brls::Label* tierCounts[4] = {};
};

class TrophyGridDataSource : public RecyclingGridDataSource {
public:
    TrophyGridDataSource(std::vector<TrophyManager::Title> titles, TrophyManager::Summary summary, bool haveSummary);

    std::vector<TrophyManager::Title> titles;
    TrophyManager::Summary summary;
    bool haveSummary = false;

    size_t getItemCount() override;
    float heightForRow(brls::View* recycler, size_t index) override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    void onItemSelected(brls::Box* recycler, size_t index) override;
    void clearData() override;
};

enum class TitleSort {
    Recent,
    Progress,
    Name,
    Earned
};

enum class TitleFilter {
    All,
    InProgress,
    Completed,
    Unstarted,
    Hidden
};

const char* titleSortLabelKey(TitleSort sort);
const char* titleFilterLabelKey(TitleFilter filter);
std::string formatRelativeAge(int64_t savedAt);

class TrophyListTab : public brls::Box {
public:
    TrophyListTab();
    ~TrophyListTab() override;

    static brls::View* create();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    static TrophyListTab* currentInstance;

private:
    BRLS_BIND(RecyclingGrid, grid, "trophies/grid");
    BRLS_BIND(PsnGatedBox, gate, "trophies/gate");
    BRLS_BIND(brls::Button, sortBtn, "trophies/sortBtn");
    BRLS_BIND(brls::Button, filterBtn, "trophies/filterBtn");
    BRLS_BIND(brls::Button, forceRefreshBtn, "trophies/forceRefreshBtn");
    BRLS_BIND(brls::Label, statusLabel, "trophies/status");

    void load(bool forceRefresh);
    void applySummary(const TrophyManager::Summary& summary);
    void applyTitles(const std::vector<TrophyManager::Title>& titles);
    void rebuildGrid();
    void showSortPicker();
    void showFilterPicker();
    void refreshControlLabels();
    void refreshStatusLine();

    TrophyManager::Summary currentSummary;
    bool haveSummary = false;
    TrophyGridDataSource* currentDataSource = nullptr;

    PsnActionButton forceRefreshGate;

    std::vector<TrophyManager::Title> titles;
    brls::RepeatingTimer statusTimer;
    static TitleSort sortMode;
    static TitleFilter filterMode;
    bool loadRequested = false;
    bool loading = false;
};

std::string formatTrophyPlatforms(const std::vector<libchiaking::trophies::TrophyPlatform>& platforms);
std::string formatTrophyCounts(const libchiaking::trophies::TrophyCounts& counts);

#endif // AKIRA_TROPHY_LIST_TAB_HPP
