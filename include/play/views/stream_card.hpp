#ifndef AKIRA_STREAM_CARD_HPP
#define AKIRA_STREAM_CARD_HPP

#include <borealis.hpp>
#include <string>

enum class CardGlyph
{
    Stats,
    Pad,
    Motion,
    Picture,
    Party,
    Chat,
    Power,
};

enum class CardState
{
    Idle,
    Active,
    Warn,
};

struct CardTheme
{
    float s = 1.0f;
    int font_ui = -1;
    int font_mono = -1;
    float alpha = 1.0f;
};

class StreamCard
{
public:
    virtual ~StreamCard() = default;

    virtual const char* id() const = 0;
    virtual std::string label() const = 0;
    virtual CardGlyph glyph() const = 0;

    virtual CardState state() const { return CardState::Idle; }
    virtual int badge() const { return 0; }
    virtual bool available() const { return true; }

    virtual bool isInstant() const { return false; }
    virtual bool closesMenu() const { return false; }
    virtual void activate() {}

    virtual float panelHeight(const CardTheme& t) const { (void)t; return 0.0f; }
    virtual std::string panelTitle() const { return {}; }
    virtual void drawPanel(NVGcontext* vg, const CardTheme& t, float x, float y, float w, float h)
    {
        (void)vg; (void)t; (void)x; (void)y; (void)w; (void)h;
    }

    virtual int optionCount() const { return 0; }
    virtual int optionIndex() const { return -1; }
    virtual std::string optionLabel(int i) const { (void)i; return {}; }
    virtual void chooseOption(int i) { (void)i; }
};

#endif // AKIRA_STREAM_CARD_HPP
