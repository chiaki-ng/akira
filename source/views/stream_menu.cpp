#include "views/stream_menu.hpp"
#include "views/stream_cards.hpp"
#include "stream/session.hpp"
#include "ui/glass.hpp"
#include "ui/theme.hpp"

#include <borealis/core/i18n.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <algorithm>
#include <cmath>
#include <format>

using namespace brls::literals;

namespace
{
    constexpr float BAR_PAD_X = 40.0f;
    constexpr float BAR_PAD_BOTTOM = 24.0f;
    constexpr float CARD_SIZE = 78.0f;
    constexpr float CARD_GAP = 11.0f;
    constexpr float CARD_RADIUS = 18.0f;
    constexpr float IDENT_W = 214.0f;
    constexpr float PANEL_GAP = 22.0f;
    constexpr float SCRIM_H = 260.0f;

    constexpr float OPEN_RISE = 54.0f;
    constexpr float CARD_RISE = 22.0f;
    constexpr float CARD_STAGGER = 0.045f;
    constexpr float CARD_DURATION = 0.26f;

    void stroked(NVGcontext* vg, NVGcolor c, float w)
    {
        nvgStrokeColor(vg, c);
        nvgStrokeWidth(vg, w);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);
    }

    float clamp01(float v)
    {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    float easeOut(float t)
    {
        float u = 1.0f - clamp01(t);
        return 1.0f - u * u * u;
    }

    float approach(float current, float target, float rate, float dt)
    {
        return current + (target - current) * (1.0f - std::exp(-rate * dt));
    }

    NVGcolor fade(NVGcolor c, float a)
    {
        c.a *= clamp01(a);
        return c;
    }

    int consoleTexture(NVGcontext* vg, bool ps5)
    {
        static int tex4 = -1;
        static int tex5 = -1;

        int& slot = ps5 ? tex5 : tex4;
        if (slot < 0)
            slot = nvgCreateImage(vg, ps5 ? "romfs:/img/console/ps5.png"
                                          : "romfs:/img/console/ps4.png", 0);
        return slot;
    }

    bool inside(float px, float py, float x, float y, float w, float h)
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }

    std::string elideText(NVGcontext* vg, const std::string& text, float maxWidth)
    {
        if (text.empty() || nvgTextBounds(vg, 0.0f, 0.0f, text.c_str(), nullptr, nullptr) <= maxWidth)
            return text;

        constexpr const char* ellipsis = "\xe2\x80\xa6";
        std::string shortened = text;
        while (!shortened.empty())
        {
            size_t start = shortened.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(shortened[start]) & 0xc0) == 0x80)
                --start;
            shortened.resize(start);

            std::string candidate = shortened + ellipsis;
            if (nvgTextBounds(vg, 0.0f, 0.0f, candidate.c_str(), nullptr, nullptr) <= maxWidth)
                return candidate;
        }
        return ellipsis;
    }
}

StreamMenu::StreamMenu()
{
    this->setWidth(brls::Application::contentWidth);
    this->setHeight(brls::Application::contentHeight);
    this->setFocusable(true);
    this->setHideHighlight(true);

    buildCards();
    registerNavigation();

    this->addGestureRecognizer(new brls::TapGestureRecognizer(
        [this](brls::TapGestureStatus status, brls::Sound* sound) {
            (void)sound;
            if (status.state != brls::GestureState::END)
                return;
            handleTap(status.position.x, status.position.y);
        }));
}

void StreamMenu::pulse(size_t index)
{
    pressIndex = index;
    pressPulse = 1.0f;
}

void StreamMenu::handleTap(float px, float py)
{
    for (const auto& r : optionHits)
    {
        if (inside(px, py, r.x, r.y, r.w, r.h))
        {
            StreamCard* card = selected();
            if (!card)
                return;
            inPanel = true;
            optionCursor = r.index;
            card->chooseOption(r.index);
            if (card->closesMenu())
                dismiss();
            return;
        }
    }

    for (const auto& r : cardHits)
    {
        if (!inside(px, py, r.x, r.y, r.w, r.h))
            continue;

        size_t index = (size_t)r.index;
        if (index >= cards.size())
            return;

        pulse(index);

        if (cards[index]->isInstant())
        {
            selection = index;
            cards[index]->activate();
            if (cards[index]->closesMenu())
                dismiss();
            return;
        }

        if (selection != index)
        {
            selection = index;
            optionCursor = -1;
        }
        inPanel = true;
        if (optionCursor < 0)
            optionCursor = cards[index]->optionIndex() >= 0 ? cards[index]->optionIndex() : 0;
        return;
    }

    if (py < panelTopY)
        dismiss();
}

void StreamMenu::buildCards()
{
    cards.clear();

    cards.push_back(std::make_unique<PowerCard>(sleepAvailable, [this](bool sleep) {
        if (onDisconnect)
            onDisconnect(sleep);
    }));

    cards.push_back(std::make_unique<StatsCard>(statsMode, [this](StatsOverlayMode m) {
        statsMode = m;
        if (onStatsToggle)
            onStatsToggle(m);
    }));

    cards.push_back(std::make_unique<PadCard>([this]() {
        if (onButtonMapping)
            onButtonMapping();
    }));

    if (onPlayers) {
        cards.push_back(std::make_unique<PlayersCard>(playersAvailable, [this]() {
            if (onPlayers)
                onPlayers();
        }));
    }

    cards.push_back(std::make_unique<MotionCard>([this]() {
        if (onGyroReset)
            onGyroReset();
    }));
}

void StreamMenu::registerNavigation()
{
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, [this](brls::View*) {
        if (inPanel)
            moveOption(-1);
        else
            moveSelection(-1);
        return true;
    }, true, true, brls::SOUND_FOCUS_SIDEBAR);

    this->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, [this](brls::View*) {
        if (inPanel)
            moveOption(1);
        else
            moveSelection(1);
        return true;
    }, true, true, brls::SOUND_FOCUS_SIDEBAR);

    this->registerAction("", brls::ControllerButton::BUTTON_NAV_UP, [this](brls::View*) {
        enterPanel();
        return true;
    }, true, false, brls::SOUND_FOCUS_CHANGE);

    this->registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, [this](brls::View*) {
        leavePanel();
        return true;
    }, true, false, brls::SOUND_FOCUS_CHANGE);

    this->registerAction("", brls::ControllerButton::BUTTON_A, [this](brls::View*) {
        fire();
        return true;
    }, false, false, brls::SOUND_CLICK);

    this->registerAction("", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        if (inPanel)
        {
            leavePanel();
            return true;
        }
        dismiss();
        return true;
    }, false, false, brls::SOUND_BACK);
}

StreamCard* StreamMenu::selected() const
{
    if (cards.empty() || selection >= cards.size())
        return nullptr;
    return cards[selection].get();
}

void StreamMenu::moveSelection(int delta)
{
    if (cards.empty())
        return;

    int n = (int)cards.size();
    int i = (int)selection;
    for (int step = 0; step < n; step++)
    {
        i = (i + delta + n) % n;
        if (cards[i]->available())
            break;
    }
    selection = (size_t)i;
    optionCursor = -1;
}

void StreamMenu::moveOption(int delta)
{
    StreamCard* card = selected();
    if (!card)
        return;

    int n = card->optionCount();
    if (n <= 0)
        return;

    if (optionCursor < 0)
        optionCursor = card->optionIndex() >= 0 ? card->optionIndex() : 0;

    optionCursor = (optionCursor + delta + n) % n;
}

void StreamMenu::enterPanel()
{
    StreamCard* card = selected();
    if (!card || card->optionCount() <= 0)
        return;

    inPanel = true;
    if (optionCursor < 0)
        optionCursor = card->optionIndex() >= 0 ? card->optionIndex() : 0;
}

void StreamMenu::leavePanel()
{
    inPanel = false;
}

void StreamMenu::fire()
{
    StreamCard* card = selected();
    if (!card || !card->available())
        return;

    if (inPanel && card->optionCount() > 0)
    {
        card->chooseOption(optionCursor < 0 ? 0 : optionCursor);
        if (card->closesMenu())
            dismiss();
        return;
    }

    if (card->isInstant())
    {
        card->activate();
        if (card->closesMenu())
            dismiss();
        return;
    }

    enterPanel();
}

void StreamMenu::dismiss()
{
    if (dismissed)
        return;
    dismissed = true;

    if (onDismiss)
        onDismiss();

    auto stack = brls::Application::getActivitiesStack();
    if (!stack.empty() && stack.back()->getContentView() == this)
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
}

void StreamMenu::setOnStatsToggle(std::function<void(StatsOverlayMode)> callback)
{
    onStatsToggle = std::move(callback);
}

void StreamMenu::setOnDisconnect(std::function<void(bool sleep)> callback)
{
    onDisconnect = std::move(callback);
}

void StreamMenu::setOnDismiss(std::function<void()> callback)
{
    onDismiss = std::move(callback);
}

void StreamMenu::setOnGyroReset(std::function<void()> callback)
{
    onGyroReset = std::move(callback);
}

void StreamMenu::setOnButtonMapping(std::function<void()> callback)
{
    onButtonMapping = std::move(callback);
}

void StreamMenu::setOnPlayers(std::function<void()> callback)
{
    onPlayers = std::move(callback);
    buildCards();
}

void StreamMenu::setStatsMode(StatsOverlayMode mode)
{
    statsMode = mode;
    buildCards();
}

void StreamMenu::setSleepAvailable(bool available)
{
    sleepAvailable = available;
    buildCards();
}

void StreamMenu::setPlayersAvailable(bool available)
{
    playersAvailable = available;
    buildCards();
}

void StreamMenu::syncGeometry(float width, float height)
{
    theme.s = height / 720.0f;

    if (theme.font_ui < 0)
        theme.font_ui = brls::Application::getFont(brls::FONT_REGULAR);
    if (theme.font_mono < 0)
    {
        theme.font_mono = brls::Application::getFont("mono");
        if (theme.font_mono < 0)
            theme.font_mono = theme.font_ui;
    }

    float s = theme.s;
    railY = height - BAR_PAD_BOTTOM * s - CARD_SIZE * s;
    railX = BAR_PAD_X * s + IDENT_W * s;
    barTop = railY - 26.0f * s;

    (void)width;
}

void StreamMenu::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    (void)style; (void)ctx;

    syncGeometry(width, height);

    if (theme.font_ui < 0)
        return;

    auto now = std::chrono::steady_clock::now();
    float dt = 1.0f / 60.0f;
    if (lastFrame != std::chrono::steady_clock::time_point{})
        dt = std::chrono::duration<float>(now - lastFrame).count();
    lastFrame = now;
    dt = std::min(dt, 0.05f);

    openT += dt;

    const akira::ui::Palette& p = akira::ui::active();
    float s = theme.s;

    float barE = easeOut(openT / CARD_DURATION);
    float barLift = (1.0f - barE) * OPEN_RISE * s;

    cardHits.clear();
    optionHits.clear();

    drawScrim(vg, x, y, width, height, barE);

    StreamCard* card = selected();
    float targetH = card ? card->panelHeight(theme) : 0.0f;
    float targetAlpha = (card && targetH > 0.0f) ? barE : 0.0f;

    if (panelH <= 0.0f)
        panelH = targetH;
    panelH = approach(panelH, targetH, 18.0f, dt);
    panelAlpha = approach(panelAlpha, targetAlpha, 16.0f, dt);

    panelTopY = y + barTop + barLift;

    if (panelH > 1.0f && panelAlpha > 0.01f)
    {
        float pw = width - BAR_PAD_X * s * 2.0f;
        float py = y + barTop - PANEL_GAP * s - panelH + barLift;
        panelTopY = py;
        theme.alpha = panelAlpha;
        drawPanel(vg, x + BAR_PAD_X * s, py, pw, panelH);
        theme.alpha = 1.0f;
    }

    drawIdentity(vg, x + BAR_PAD_X * s, y + railY + barLift, barE, p);
    drawRail(vg, x + railX, y + railY, barLift, dt, p);
    drawHints(vg, x + width - BAR_PAD_X * s,
              y + railY + CARD_SIZE * s * 0.5f + barLift, barE, p);

    pressPulse = approach(pressPulse, 0.0f, 12.0f, dt);
}

void StreamMenu::drawScrim(NVGcontext* vg, float x, float y, float w, float h, float e)
{
    const akira::ui::Palette& p = akira::ui::active();
    float s = theme.s;
    float top = h - SCRIM_H * s;

    NVGcolor clear = akira::ui::withAlpha(p.gradientBottom, 0);
    NVGcolor mid = fade(akira::ui::withAlpha(p.gradientBottom, 0xb8), e);
    NVGcolor deep = fade(akira::ui::withAlpha(p.backgroundDeep, 0xf0), e);

    float split = SCRIM_H * s * 0.42f;

    NVGpaint upper = nvgLinearGradient(vg, x, y + top, x, y + top + split, clear, mid);
    nvgBeginPath(vg);
    nvgRect(vg, x, y + top, w, split);
    nvgFillPaint(vg, upper);
    nvgFill(vg);

    NVGpaint lower = nvgLinearGradient(vg, x, y + top + split, x, y + h, mid, deep);
    nvgBeginPath(vg);
    nvgRect(vg, x, y + top + split, w, SCRIM_H * s - split);
    nvgFillPaint(vg, lower);
    nvgFill(vg);
}

void StreamMenu::drawIdentity(NVGcontext* vg, float x, float y, float e, const akira::ui::Palette& p)
{
    float s = theme.s;
    float box = 46.0f * s;
    float cy = y + CARD_SIZE * s * 0.5f;
    float by = cy - box * 0.5f;

    akira::ui::drawGlassSurface(vg, x, by, box, box, 13.0f * s, e, true, p, s);

    int tex = consoleTexture(vg, consolePs5);
    if (tex >= 0)
    {
        int iw = 0;
        int ih = 0;
        nvgImageSize(vg, tex, &iw, &ih);

        if (iw > 0 && ih > 0)
        {
            float fit = box - 8.0f * s;
            float scale = std::min(fit / (float)iw, fit / (float)ih);
            float dw = iw * scale;
            float dh = ih * scale;
            float dx = x + (box - dw) * 0.5f;
            float dy = by + (box - dh) * 0.5f;

            NVGpaint img = nvgImagePattern(vg, dx, dy, dw, dh, 0.0f, tex, clamp01(e));
            nvgBeginPath(vg);
            nvgRect(vg, dx, dy, dw, dh);
            nvgFillPaint(vg, img);
            nvgFill(vg);
        }
    }

    Session* session = Session::GetInstance();
    StreamStats st = session ? session->getStreamStats() : StreamStats{};

    float tx = x + box + 13.0f * s;

    nvgFontFaceId(vg, theme.font_ui);
    nvgFontSize(vg, 16.0f * s);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
    nvgFillColor(vg, fade(p.text, e));
    const float nameWidth = IDENT_W * s - (tx - x) - 14.0f * s;
    const std::string displayName = elideText(vg, consoleName, nameWidth);
    nvgText(vg, tx, cy - 1.0f * s, displayName.c_str(), nullptr);

    float dotR = 3.0f * s;
    float my = cy + 7.0f * s;

    NVGcolor health = p.success;
    if (st.packet_loss_percent >= 2.0f)
        health = p.danger;
    else if (st.packet_loss_percent >= 0.5f)
        health = p.warning;

    nvgBeginPath(vg);
    nvgCircle(vg, tx + dotR, my, dotR);
    nvgFillColor(vg, fade(health, e));
    nvgFill(vg);

    uint64_t mins = st.stream_duration_seconds / 60;
    uint64_t secs = st.stream_duration_seconds % 60;
    int shownH = st.video_height > 0 ? st.video_height : st.requested_height;
    std::string meta = std::format("{}p{} \xc2\xb7 {}m{:02}s", shownH, st.requested_fps, mins, secs);

    nvgFontSize(vg, 12.5f * s);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, fade(p.textMuted, e));
    nvgText(vg, tx + dotR * 2.0f + 7.0f * s, my, meta.c_str(), nullptr);
}

void StreamMenu::drawRail(NVGcontext* vg, float x, float y, float lift, float dt,
                          const akira::ui::Palette& p)
{
    float s = theme.s;
    float step = (CARD_SIZE + CARD_GAP) * s;

    float targetSelX = x + (float)selection * step;
    if (selX < 0.0f)
        selX = targetSelX;
    selX = approach(selX, targetSelX, 24.0f, dt);

    if (!inPanel)
    {
        float size = CARD_SIZE * s;
        float r = CARD_RADIUS * s;
        float grow = 3.0f * s;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, selX - grow, y + lift - grow,
                       size + grow * 2.0f, size + grow * 2.0f, r + grow);
        nvgFillColor(vg, akira::ui::withAlpha(p.focusA, 0x42));
        nvgFill(vg);
    }

    for (size_t i = 0; i < cards.size(); i++)
    {
        float ce = easeOut((openT - 0.04f - (float)i * CARD_STAGGER) / CARD_DURATION);
        float cy = y + lift + (1.0f - ce) * CARD_RISE * s;
        float cx = x + (float)i * step;

        drawCard(vg, cards[i].get(), cx, cy, i == selection, ce, i, p);

        if (cards[i]->available())
            cardHits.push_back({ cx - 6.0f * s, cy - 6.0f * s,
                                 CARD_SIZE * s + 12.0f * s, CARD_SIZE * s + 12.0f * s, (int)i });
    }
}

void StreamMenu::drawCard(NVGcontext* vg, StreamCard* card, float x, float y, bool sel,
                          float e, size_t index, const akira::ui::Palette& p)
{
    float s = theme.s;
    float size = CARD_SIZE * s;
    float r = CARD_RADIUS * s;

    float squash = 0.0f;
    if (index == pressIndex && pressPulse > 0.01f)
        squash = pressPulse * 4.0f * s;

    float cx = x + squash * 0.5f;
    float cy = y + squash * 0.5f;
    float cs = size - squash;

    akira::ui::drawGlassSurface(vg, cx, cy, cs, cs, r, e, sel, p, s);

    const bool available = card->available();
    NVGcolor tint = available ? p.textMuted : akira::ui::withAlpha(p.textDim, 0x62);
    if (sel && available)
        tint = p.accent;
    else if (card->state() == CardState::Active)
        tint = p.success;
    else if (card->state() == CardState::Warn)
        tint = p.warning;

    float gs = 25.0f * s;
    drawGlyph(vg, card->glyph(), cx + (cs - gs) * 0.5f, cy + cs * 0.24f, gs, fade(tint, e));

    std::string label = card->label();
    nvgFontFaceId(vg, theme.font_ui);
    nvgFontSize(vg, 11.0f * s);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, fade(!available ? akira::ui::withAlpha(p.textDim, 0x68)
                                       : (sel ? p.text : p.textMuted), e));
    nvgText(vg, cx + cs * 0.5f, cy + cs * 0.76f, label.c_str(), nullptr);

    int badge = card->badge();
    if (badge > 0)
        drawBadge(vg, cx + cs - 6.0f * s, cy + 6.0f * s, badge, e, p);
}

void StreamMenu::drawBadge(NVGcontext* vg, float cx, float cy, int count, float e,
                           const akira::ui::Palette& p)
{
    float s = theme.s;
    float r = 10.0f * s;

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r + 2.0f * s);
    nvgFillColor(vg, fade(p.backgroundDeep, e));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r);
    nvgFillColor(vg, fade(p.danger, e));
    nvgFill(vg);

    std::string text = count > 99 ? "99" : std::format("{}", count);
    nvgFontFaceId(vg, theme.font_ui);
    nvgFontSize(vg, 11.5f * s);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, fade(nvgRGBA(255, 255, 255, 255), e));
    nvgText(vg, cx, cy, text.c_str(), nullptr);
}

void StreamMenu::drawGlyph(NVGcontext* vg, CardGlyph g, float x, float y, float s, NVGcolor c)
{
    switch (g)
    {
        case CardGlyph::Stats:
        {
            const float bars[4] = { 0.42f, 0.78f, 0.30f, 0.60f };
            for (int i = 0; i < 4; i++)
            {
                float bx = x + s * (0.14f + i * 0.24f);
                nvgBeginPath(vg);
                nvgMoveTo(vg, bx, y + s * 0.86f);
                nvgLineTo(vg, bx, y + s * 0.86f - s * bars[i]);
                stroked(vg, c, s * 0.13f);
            }
            break;
        }
        case CardGlyph::Pad:
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + s * 0.06f, y + s * 0.28f, s * 0.88f, s * 0.44f, s * 0.20f);
            stroked(vg, c, s * 0.08f);

            nvgBeginPath(vg);
            nvgMoveTo(vg, x + s * 0.22f, y + s * 0.50f);
            nvgLineTo(vg, x + s * 0.38f, y + s * 0.50f);
            nvgMoveTo(vg, x + s * 0.30f, y + s * 0.42f);
            nvgLineTo(vg, x + s * 0.30f, y + s * 0.58f);
            stroked(vg, c, s * 0.075f);

            nvgBeginPath(vg);
            nvgCircle(vg, x + s * 0.70f, y + s * 0.46f, s * 0.06f);
            nvgFillColor(vg, c);
            nvgFill(vg);
            break;
        }
        case CardGlyph::Motion:
        {
            float cx = x + s * 0.5f;
            float cy = y + s * 0.54f;
            float r = s * 0.33f;
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, r, -0.5f, 4.35f, NVG_CW);
            stroked(vg, c, s * 0.09f);

            nvgBeginPath(vg);
            nvgMoveTo(vg, cx + r * 0.30f, cy - r * 1.20f);
            nvgLineTo(vg, cx + r * 1.02f, cy - r * 0.78f);
            nvgLineTo(vg, cx + r * 0.28f, cy - r * 0.42f);
            stroked(vg, c, s * 0.09f);
            break;
        }
        case CardGlyph::Picture:
        {
            const float xs[3] = { 0.22f, 0.50f, 0.78f };
            const float knob[3] = { 0.36f, 0.64f, 0.46f };
            for (int i = 0; i < 3; i++)
            {
                float lx = x + s * xs[i];
                nvgBeginPath(vg);
                nvgMoveTo(vg, lx, y + s * 0.16f);
                nvgLineTo(vg, lx, y + s * 0.86f);
                stroked(vg, c, s * 0.08f);

                nvgBeginPath(vg);
                nvgCircle(vg, lx, y + s * knob[i], s * 0.11f);
                nvgFillColor(vg, c);
                nvgFill(vg);
            }
            break;
        }
        case CardGlyph::Party:
        {
            nvgBeginPath(vg);
            nvgCircle(vg, x + s * 0.36f, y + s * 0.32f, s * 0.14f);
            stroked(vg, c, s * 0.08f);

            nvgBeginPath(vg);
            nvgArc(vg, x + s * 0.36f, y + s * 0.82f, s * 0.27f, 3.34f, 6.08f, NVG_CW);
            stroked(vg, c, s * 0.08f);

            nvgBeginPath(vg);
            nvgArc(vg, x + s * 0.60f, y + s * 0.44f, s * 0.17f, -1.0f, 1.0f, NVG_CW);
            stroked(vg, c, s * 0.075f);

            nvgBeginPath(vg);
            nvgArc(vg, x + s * 0.60f, y + s * 0.44f, s * 0.32f, -1.0f, 1.0f, NVG_CW);
            stroked(vg, c, s * 0.075f);
            break;
        }
        case CardGlyph::Chat:
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + s * 0.08f, y + s * 0.20f, s * 0.84f, s * 0.50f, s * 0.16f);
            stroked(vg, c, s * 0.08f);

            nvgBeginPath(vg);
            nvgMoveTo(vg, x + s * 0.30f, y + s * 0.68f);
            nvgLineTo(vg, x + s * 0.25f, y + s * 0.88f);
            nvgLineTo(vg, x + s * 0.50f, y + s * 0.70f);
            stroked(vg, c, s * 0.08f);
            break;
        }
        case CardGlyph::Power:
        {
            float cx = x + s * 0.5f;
            float cy = y + s * 0.56f;
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, s * 0.33f, -0.85f, 3.14f + 0.85f, NVG_CW);
            stroked(vg, c, s * 0.09f);

            nvgBeginPath(vg);
            nvgMoveTo(vg, cx, y + s * 0.10f);
            nvgLineTo(vg, cx, y + s * 0.48f);
            stroked(vg, c, s * 0.09f);
            break;
        }
    }
}

void StreamMenu::drawPanel(NVGcontext* vg, float x, float y, float w, float h)
{
    const akira::ui::Palette& p = akira::ui::active();
    float s = theme.s;
    float a = theme.alpha;

    StreamCard* card = selected();
    if (!card)
        return;

    akira::ui::drawGlassSurface(vg, x, y, w, h, 20.0f * s, a, false, p, s);

    nvgSave(vg);
    nvgScissor(vg, x, y, w, h);

    float padX = 28.0f * s;
    float headY = y + 24.0f * s;

    std::string title = card->panelTitle();
    nvgFontFaceId(vg, theme.font_ui);
    nvgFontSize(vg, 19.0f * s);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, fade(p.text, a));
    nvgText(vg, x + padX, headY, title.c_str(), nullptr);

    float ruleY = headY + 32.0f * s;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + padX, ruleY);
    nvgLineTo(vg, x + w - padX, ruleY);
    stroked(vg, fade(p.surfaceLine, a), 1.0f);

    float bodyY = ruleY + 18.0f * s;

    if (card->optionCount() > 0)
    {
        drawOptions(vg, x + padX, bodyY, w - padX * 2.0f);
        bodyY += 52.0f * s;
    }

    card->drawPanel(vg, theme, x + padX, bodyY, w - padX * 2.0f, y + h - bodyY);

    nvgRestore(vg);
}

void StreamMenu::drawOptions(NVGcontext* vg, float x, float y, float w)
{
    (void)w;

    const akira::ui::Palette& p = akira::ui::active();
    float s = theme.s;
    float a = theme.alpha;

    StreamCard* card = selected();
    if (!card)
        return;

    int n = card->optionCount();
    int active = card->optionIndex();
    float cursor = x;

    for (int i = 0; i < n; i++)
    {
        std::string label = card->optionLabel(i);

        nvgFontFaceId(vg, theme.font_ui);
        nvgFontSize(vg, 14.0f * s);
        float tw = nvgTextBounds(vg, 0, 0, label.c_str(), nullptr, nullptr);
        float bw = tw + 44.0f * s;
        float bh = 34.0f * s;

        bool isActive = (i == active);
        bool isCursor = inPanel && i == optionCursor;

        if (isCursor)
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cursor - 3.0f * s, y - 3.0f * s,
                           bw + 6.0f * s, bh + 6.0f * s, 14.0f * s);
            nvgFillColor(vg, fade(akira::ui::withAlpha(p.focusA, 0x42), a));
            nvgFill(vg);
        }

        akira::ui::drawGlassSurface(vg, cursor, y, bw, bh, 11.0f * s, a,
                                    isActive || isCursor, p, s);

        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, fade((isActive || isCursor) ? p.text : p.textMuted, a));
        nvgText(vg, cursor + bw * 0.5f, y + bh * 0.5f, label.c_str(), nullptr);

        if (a > 0.6f)
            optionHits.push_back({ cursor - 4.0f * s, y - 4.0f * s,
                                   bw + 8.0f * s, bh + 8.0f * s, i });

        cursor += bw + 8.0f * s;
    }
}

void StreamMenu::drawHints(NVGcontext* vg, float right, float y, float e,
                           const akira::ui::Palette& p)
{
    float s = theme.s;
    StreamCard* card = selected();

    struct Hint { const char* glyph; std::string text; };
    std::vector<Hint> hints;

    if (inPanel)
    {
        hints.push_back({ "A", "akira/stream_menu/hint_select"_i18n });
        hints.push_back({ "B", "akira/stream_menu/hint_back"_i18n });
    }
    else
    {
        bool instant = card && card->isInstant();
        hints.push_back({ "A", instant ? "akira/stream_menu/hint_select"_i18n
                                       : "akira/stream_menu/hint_open"_i18n });
        hints.push_back({ "B", "akira/stream_menu/hint_resume"_i18n });
    }

    float cursor = right;

    for (auto it = hints.rbegin(); it != hints.rend(); ++it)
    {
        nvgFontFaceId(vg, theme.font_ui);
        nvgFontSize(vg, 12.5f * s);
        float tw = nvgTextBounds(vg, 0, 0, it->text.c_str(), nullptr, nullptr);

        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, fade(p.textMuted, e));
        nvgText(vg, cursor, y, it->text.c_str(), nullptr);

        float r = 9.5f * s;
        float gx = cursor - tw - 7.0f * s - r;

        nvgBeginPath(vg);
        nvgCircle(vg, gx, y, r);
        stroked(vg, fade(p.textDim, e), 1.0f);

        nvgFontSize(vg, 10.5f * s);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, fade(p.textMuted, e));
        nvgText(vg, gx, y, it->glyph, nullptr);

        cursor = gx - r - 18.0f * s;
    }
}
