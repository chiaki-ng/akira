#ifndef AKIRA_STREAM_MENU_HPP
#define AKIRA_STREAM_MENU_HPP

#include "stream/stream_stats.hpp"
#include "views/stream_card.hpp"
#include <borealis.hpp>
#include <chrono>
#include "ui/theme.hpp"
#include <functional>
#include <memory>
#include <vector>

class StreamMenu : public brls::Box
{
public:
    StreamMenu();

    void setOnStatsToggle(std::function<void(StatsOverlayMode)> callback);
    void setOnDisconnect(std::function<void(bool sleep)> callback);
    void setOnDismiss(std::function<void()> callback);
    void setOnGyroReset(std::function<void()> callback);
    void setOnButtonMapping(std::function<void()> callback);
    void setOnPlayers(std::function<void()> callback);

    void setStatsMode(StatsOverlayMode mode);
    void setSleepAvailable(bool available);
    void setPlayersAvailable(bool available);
    void setConsoleName(const std::string& name) { consoleName = name; }
    void setConsoleIsPs5(bool ps5) { consolePs5 = ps5; }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    bool isTranslucent() override { return true; }

private:
    void buildCards();
    void registerNavigation();

    void moveSelection(int delta);
    void moveOption(int delta);
    void enterPanel();
    void leavePanel();
    void fire();
    void dismiss();

    StreamCard* selected() const;
    void syncGeometry(float width, float height);

    void drawScrim(NVGcontext* vg, float x, float y, float w, float h, float e);
    void drawIdentity(NVGcontext* vg, float x, float y, float e, const akira::ui::Palette& p);
    void drawRail(NVGcontext* vg, float x, float y, float lift, float dt, const akira::ui::Palette& p);
    void drawCard(NVGcontext* vg, StreamCard* card, float x, float y, bool sel, float e,
                  size_t index, const akira::ui::Palette& p);
    void drawGlyph(NVGcontext* vg, CardGlyph g, float x, float y, float s, NVGcolor c);
    void drawBadge(NVGcontext* vg, float cx, float cy, int count, float e, const akira::ui::Palette& p);
    void drawPanel(NVGcontext* vg, float x, float y, float w, float h);
    void drawOptions(NVGcontext* vg, float x, float y, float w);
    void drawHints(NVGcontext* vg, float right, float y, float e, const akira::ui::Palette& p);

    void handleTap(float px, float py);
    void pulse(size_t index);

    bool dismissed = false;

    struct HitRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        int index = 0;
    };

    std::vector<std::unique_ptr<StreamCard>> cards;
    size_t selection = 0;
    int optionCursor = -1;
    bool inPanel = false;
    bool sleepAvailable = true;
    bool playersAvailable = true;
    std::string consoleName = "PlayStation";
    bool consolePs5 = false;

    StatsOverlayMode statsMode = StatsOverlayMode::Off;

    CardTheme theme;
    float railY = 0.0f;
    float railX = 0.0f;
    float barTop = 0.0f;

    std::vector<HitRect> cardHits;
    std::vector<HitRect> optionHits;
    float panelTopY = 0.0f;

    std::chrono::steady_clock::time_point lastFrame{};
    float openT = 0.0f;
    float selX = -1.0f;
    float panelAlpha = 0.0f;
    float panelH = 0.0f;
    float pressPulse = 0.0f;
    size_t pressIndex = 0;

    std::function<void(StatsOverlayMode)> onStatsToggle;
    std::function<void(bool)> onDisconnect;
    std::function<void()> onDismiss;
    std::function<void()> onGyroReset;
    std::function<void()> onButtonMapping;
    std::function<void()> onPlayers;
};

#endif // AKIRA_STREAM_MENU_HPP
