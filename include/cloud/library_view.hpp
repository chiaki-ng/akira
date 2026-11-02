#ifndef AKIRA_CLOUD_LIBRARY_VIEW_HPP
#define AKIRA_CLOUD_LIBRARY_VIEW_HPP

#include <borealis.hpp>

#include <memory>
#include <optional>
#include <vector>
#include <string>

#include "cloud/fetch_progress.hpp"
#include "cloud/library_listing.hpp"
#include "cloud/service.hpp"
#include "views/vendored/switchfin/recycling_grid.hpp"

namespace cloud {

class LibraryView : public brls::Box {
public:
    LibraryView();
    ~LibraryView() override;

    void willAppear(bool resetState) override;

private:
    void refresh(bool force);
    void renderSnapshot(const Snapshot& snapshot);
    void showState(const Snapshot& snapshot);
    void showFetch(uint64_t operation, const FetchProgress& progress);
    void addLoadingLines(brls::Box* panel, const std::string& waitingText);
    void writeLoadingLines();
    void dropLoadingLines();
    void showCatalog(const Snapshot& snapshot);
    void applyFilter();
    void rebuildFilterControls();
    void pickFilter(LibraryFilter filter);
    void pickPlatform(PlatformFilter platform);
    void updateSearchButton();
    void scheduleUnlock();
    void openSignIn();
    void openDetail(const Game& game);
    void toggleShortcut(const Game& game);

    static auto filterTitle(LibraryFilter filter) -> std::string;
    static auto platformTitle(PlatformFilter platform) -> std::string;
    static void styleChip(brls::Button* chip, bool selected);

    LibraryListing listing;
    brls::Box* controlsRow = nullptr;
    brls::Box* filterRow = nullptr;
    brls::Box* platformRow = nullptr;
    std::vector<LibraryFilter> shownFilters;
    std::vector<brls::Button*> filterChips;
    std::vector<brls::Button*> platformChips;
    brls::Button* searchButton = nullptr;
    brls::Button* sortButton = nullptr;
    brls::Button* overflowButton = nullptr;
    int sortState = 0;
    uint64_t unlockGeneration = 0;

    void openSortPicker();
    void openOverflowMenu();
    void clearCatalogCache();
    void updateSortButton();
    void showAddGameDialog(const Game& game);

    brls::Box* statusChip = nullptr;
    brls::Label* statusChipLabel = nullptr;
    brls::Label* countLabel = nullptr;
    bool canPairState = false;
    bool checkingState = false;
    RecyclingGrid* grid = nullptr;
    brls::Box* stateBox = nullptr;
    RecyclingGridDataSource* dataSource = nullptr;

    struct LoadingLines {
        brls::Box* box = nullptr;
        brls::Label* step = nullptr;
        brls::Label* detail = nullptr;
        brls::Label* pages = nullptr;
        std::string waitingText;
    };

    FollowedFetch fetch;
    std::optional<LoadingLines> loadingLines;
    brls::Animatable stepFade{1.0f};

    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    uint64_t generation = 0;
};

} // namespace cloud

#endif // AKIRA_CLOUD_LIBRARY_VIEW_HPP
