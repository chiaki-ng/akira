#ifndef AKIRA_TROPHY_DETAIL_VIEW_HPP
#define AKIRA_TROPHY_DETAIL_VIEW_HPP

#include <borealis.hpp>

#include <string>
#include <unordered_set>
#include <vector>

#include "core/trophy_manager.hpp"
#include "views/progress_ring.hpp"
#include "views/psn_action_button.hpp"
#include "views/psn_gated_box.hpp"
#include "views/vendored/switchfin/recycling_grid.hpp"

class TrophyRowCell : public RecyclingGridItem {
public:
    TrophyRowCell();
    ~TrophyRowCell() override;

    void bindTrophy(const libchiaking::trophies::Trophy& trophy);
    void prepareForReuse() override;
    void cacheForReuse() override;

    static RecyclingGridItem* create();

private:
    brls::Image* icon = nullptr;
    brls::Label* nameLabel = nullptr;
    brls::Label* detailLabel = nullptr;
    brls::Label* rarityLabel = nullptr;
    brls::Label* stateLabel = nullptr;
    brls::Box* progressTrack = nullptr;
    brls::Rectangle* progressFill = nullptr;

    std::string iconUrl;

    static std::unordered_set<TrophyRowCell*> liveCells;
    static std::unordered_set<std::string> retriedIcons;
};

class SummaryCell : public RecyclingGridItem {
public:
    SummaryCell();

    void bindSummary(int earned, int available, int progress, const libchiaking::trophies::TrophyCounts& counts);

    static RecyclingGridItem* create();

private:
    ProgressRing* ring = nullptr;
    brls::Label* ringLabel = nullptr;
    brls::Label* earnedValue = nullptr;
    brls::Label* availableValue = nullptr;
    brls::Image* tierImages[4] = {};
    brls::Label* tierCounts[4] = {};

    brls::Animatable earnedAnim{0.0f};
    brls::Animatable availAnim{0.0f};
    brls::Animatable ringLabelAnim{0.0f};
    brls::Animatable tierAnim[4] = {};
    int prevEarned = -1;
    int prevAvail = -1;
    int prevRingPct = -1;
    int prevTier[4] = {-1, -1, -1, -1};
};

class TrophyDetailSource : public RecyclingGridDataSource {
public:
    TrophyDetailSource(int earned, int available, int progress,
        libchiaking::trophies::TrophyCounts counts, std::vector<libchiaking::trophies::Trophy> trophies);

    size_t getItemCount() override;
    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override;
    float heightForRow(brls::View* recycler, size_t index) override;
    void clearData() override;

private:
    int earned;
    int available;
    int progress;
    libchiaking::trophies::TrophyCounts counts;
    std::vector<libchiaking::trophies::Trophy> trophies;
};

enum class TrophySort {
    Progress,
    DateObtained,
    Grade,
    Rarity,
    Name
};

enum class TrophyFilter {
    All,
    Earned,
    Unearned,
    Hidden
};

const char* trophySortLabelKey(TrophySort sort);
const char* trophyFilterLabelKey(TrophyFilter filter);
bool trophyMatchesFilter(const libchiaking::trophies::Trophy& trophy, TrophyFilter filter);
void sortTrophies(std::vector<libchiaking::trophies::Trophy>& trophies, TrophySort sort);

class TrophyDetailView : public brls::Box {
public:
    explicit TrophyDetailView(const TrophyManager::Title& title);
    ~TrophyDetailView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    static TrophyDetailView* currentInstance;

private:
    BRLS_BIND(brls::Label, titleLabel, "detail/title");
    BRLS_BIND(brls::Label, subtitleLabel, "detail/subtitle");
    BRLS_BIND(brls::Label, countsLabel, "detail/counts");
    BRLS_BIND(brls::Button, groupBtn, "detail/groupBtn");
    BRLS_BIND(brls::Button, sortBtn, "detail/sortBtn");
    BRLS_BIND(brls::Button, filterBtn, "detail/filterBtn");
    BRLS_BIND(brls::Button, refreshBtn, "detail/refreshBtn");
    BRLS_BIND(RecyclingGrid, list, "detail/list");
    BRLS_BIND(PsnGatedBox, gate, "detail/gate");

    void load(bool forceRefresh);
    void applyDetail(const TrophyManager::TitleTrophies& detail);
    void applyGroup(size_t index);
    void showGroupPicker();
    void showSortPicker();
    void showFilterPicker();
    void refreshControlLabels();

    TrophyManager::Title title;
    std::vector<libchiaking::trophies::TrophyGroup> groups;
    std::vector<libchiaking::trophies::Trophy> trophies;
    PsnActionButton refreshGate;

    size_t selectedGroup = 0;
    static TrophySort sortMode;
    static TrophyFilter filterMode;
    bool loadRequested = false;
    bool loading = false;
};

std::string formatTrophyRarity(const libchiaking::trophies::Trophy& trophy);

#endif // AKIRA_TROPHY_DETAIL_VIEW_HPP
