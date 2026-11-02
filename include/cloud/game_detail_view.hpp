#ifndef AKIRA_CLOUD_GAME_DETAIL_VIEW_HPP
#define AKIRA_CLOUD_GAME_DETAIL_VIEW_HPP

#include <borealis.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include "cloud/game_actions.hpp"
#include "cloud/launch_checks.hpp"
#include "cloud/models.hpp"
#include "cloud/service.hpp"

namespace cloud {

class GameDetailView : public brls::Box {
public:
    GameDetailView(Game game, std::vector<Game> listed);
    ~GameDetailView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    brls::View* getDefaultFocus() override;

private:
    class StageShade : public brls::View {
    public:
        void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                  brls::FrameContext* ctx) override;
    };

    class CheckRow : public brls::View {
    public:
        CheckRow();
        void show(std::optional<LaunchChecks> checks);
        void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                  brls::FrameContext* ctx) override;

    private:
        static auto label(LaunchChecks::Step step) -> std::string;
        std::optional<LaunchChecks> shown;
    };

    enum class TitleRefreshPhase {
        Idle,
        Checking,
        CoolingDown
    };

    void buildStage();
    void buildBand();
    void loadDetails();
    void showDetail(const GameDetail& detail);
    void loadImage(const std::string& url, int width, brls::Image* into);
    void startCardCheck();
    void render();
    void renderMetadata();
    void renderActions();
    void renderNotes();
    void renderBand(const GameDetail& detail);
    void updateRelease();
    void checkAndPlay(const Game& target);
    void chooseBundleGame();
    void showStorePage(bool addingToLibrary);
    void refreshTitle();
    void tickRefreshCooldown();
    auto refreshText() const -> std::string;
    auto refreshSecondsLeft() const -> int;
    auto actions() const -> GameActions;
    auto bundleChoices() const -> std::vector<Game>;
    auto focusedAction() const -> std::optional<GameActions::Focus>;
    auto actionButton(GameActions::Focus focus) const -> brls::Button*;

    static auto standingText(Standing standing, const Game& game) -> std::string;
    static auto cardCheckText(const CardCheck& check) -> std::optional<std::string>;
    static auto releaseText(const Game& game, int64_t nowMs) -> std::optional<std::string>;
    static auto metadataLabel(const std::string& text, NVGcolor color) -> brls::Label*;
    static auto nowMs() -> int64_t;
    static auto checkOf(libchiaking::catalog::LaunchCheck check) -> LaunchChecks::Check;

    static constexpr float kStageHeight = 500.0f;
    static constexpr float kInset = 64.0f;
    static constexpr int kBackdropWidth = 1280;
    static constexpr int kLogoWidth = 640;
    static constexpr std::chrono::seconds kTitleRefreshCooldown{15};

    Game game;
    std::vector<Game> listed;
    std::optional<CardCheck> cardCheck;
    std::optional<LaunchChecks> checks;
    std::optional<std::string> titleNotice;
    bool titleNoticeAdded = false;
    TitleRefreshPhase titleRefresh = TitleRefreshPhase::Idle;
    std::chrono::steady_clock::time_point refreshReadyAt{};
    int refreshShownSecond = -1;
    bool parkingFocus = false;
    uint64_t attempts = 0;
    uint64_t refreshAttempt = 0;

    libchiaking::platform::StopSource lifetime;
    libchiaking::platform::StopSource launchStop;

    brls::Box* stage = nullptr;
    brls::Image* backdrop = nullptr;
    brls::Image* hero = nullptr;
    brls::Image* logo = nullptr;
    brls::Label* titleLabel = nullptr;
    brls::Box* metadataRow = nullptr;
    brls::Label* releaseLabel = nullptr;
    brls::Box* actionsRow = nullptr;
    CheckRow* checkRow = nullptr;
    brls::Label* reasonLabel = nullptr;
    brls::Label* hintLabel = nullptr;
    brls::Label* noticeLabel = nullptr;
    brls::Box* band = nullptr;
    std::vector<std::pair<GameActions::Focus, brls::Button*>> buttons;
    std::string releaseShown;

    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace cloud

#endif // AKIRA_CLOUD_GAME_DETAIL_VIEW_HPP
