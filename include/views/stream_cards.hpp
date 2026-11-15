#ifndef AKIRA_STREAM_CARDS_HPP
#define AKIRA_STREAM_CARDS_HPP

#include "stream/stream_stats.hpp"
#include "views/stream_card.hpp"
#include <functional>

class StatsCard : public StreamCard
{
public:
    StatsCard(StatsOverlayMode mode, std::function<void(StatsOverlayMode)> apply);

    const char* id() const override { return "stats"; }
    std::string label() const override;
    CardGlyph glyph() const override { return CardGlyph::Stats; }
    CardState state() const override;

    float panelHeight(const CardTheme& t) const override;
    std::string panelTitle() const override;
    void drawPanel(NVGcontext* vg, const CardTheme& t, float x, float y, float w, float h) override;

    bool closesMenu() const override { return true; }

    int optionCount() const override { return 3; }
    int optionIndex() const override { return (int)mode; }
    std::string optionLabel(int i) const override;
    void chooseOption(int i) override;

private:
    StatsOverlayMode mode;
    std::function<void(StatsOverlayMode)> apply;
};

class PadCard : public StreamCard
{
public:
    explicit PadCard(std::function<void()> open) : open(std::move(open)) {}

    const char* id() const override { return "pad"; }
    std::string label() const override;
    CardGlyph glyph() const override { return CardGlyph::Pad; }
    bool isInstant() const override { return true; }
    void activate() override { if (open) open(); }

private:
    std::function<void()> open;
};

class MotionCard : public StreamCard
{
public:
    explicit MotionCard(std::function<void()> reset) : reset(std::move(reset)) {}

    const char* id() const override { return "motion"; }
    std::string label() const override;
    CardGlyph glyph() const override { return CardGlyph::Motion; }
    bool isInstant() const override { return true; }
    bool closesMenu() const override { return true; }
    void activate() override { if (reset) reset(); }

private:
    std::function<void()> reset;
};

class PlayersCard : public StreamCard
{
public:
    PlayersCard(bool available, std::function<void()> open)
        : enabled(available), open(std::move(open)) {}

    const char* id() const override { return "players"; }
    std::string label() const override;
    CardGlyph glyph() const override { return CardGlyph::Party; }
    bool available() const override { return enabled; }
    bool isInstant() const override { return true; }
    void activate() override { if (open) open(); }

private:
    bool enabled;
    std::function<void()> open;
};

class PowerCard : public StreamCard
{
public:
    PowerCard(bool sleepAvailable, std::function<void(bool)> disconnect);

    const char* id() const override { return "power"; }
    std::string label() const override;
    CardGlyph glyph() const override { return CardGlyph::Power; }

    float panelHeight(const CardTheme& t) const override;
    std::string panelTitle() const override;

    int optionCount() const override { return sleepAvailable ? 2 : 1; }
    int optionIndex() const override { return -1; }
    std::string optionLabel(int i) const override;
    void chooseOption(int i) override;

private:
    bool sleepAvailable;
    std::function<void(bool)> disconnect;
};

#endif // AKIRA_STREAM_CARDS_HPP
