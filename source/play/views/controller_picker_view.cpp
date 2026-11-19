#include "play/views/controller_picker_view.hpp"
#include "log/log.hpp"

#include <string>
#include <vector>

#include "play/views/battery_pips.hpp"
#include "play/views/pad_art.hpp"

#include "app/views/glass.hpp"
#include "app/views/theme.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::play::views
{

using namespace brls::literals;
using akira::platform::PadDescription;
using akira::platform::PadPathKind;

bool ControllerPickerView::Differ(const PadDescription& a, const PadDescription& b)
{
    if (a.kind != b.kind)
        return true;
    if (a.caps.analog_triggers != b.caps.analog_triggers)
        return true;
    if (a.caps.touchpad != b.caps.touchpad)
        return true;
    return false;
}

std::vector<std::string> ControllerPickerView::CapabilityLabels(const PadDescription& d)
{
    std::vector<std::string> out;

    if (d.caps.touchpad)        out.emplace_back("Touchpad");
    if (d.caps.analog_triggers) out.emplace_back("Triggers");
    if (d.caps.gyro)            out.emplace_back("Motion");
    if (out.empty())            out.emplace_back("Digital input");

    return out;
}

brls::Box* ControllerPickerView::MakeCapabilityChip(const std::string& text)
{
    const auto& p = app::views::active();
    auto* chip = new brls::Box();
    chip->setAxis(brls::Axis::ROW);
    chip->setJustifyContent(brls::JustifyContent::CENTER);
    chip->setAlignItems(brls::AlignItems::CENTER);
    chip->setHeight(27);
    chip->setPaddingLeft(10);
    chip->setPaddingRight(10);
    chip->setMarginLeft(4);
    chip->setMarginRight(4);
    chip->setCornerRadius(13.5f);
    chip->setBackgroundColor(app::views::withAlpha(p.surface, 0x50));
    chip->setBorderThickness(1.0f);
    chip->setBorderColor(app::views::withAlpha(p.focusB, 0x28));

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(13);
    label->setTextColor(p.textMuted);
    chip->addView(label);
    return chip;
}

std::string ControllerPickerView::SlotLine(HidNpadIdType npad)
{
    if (npad == HidNpadIdType_Handheld)
        return "Handheld";
    return "Player " + std::to_string((int)npad + 1);
}

bool ControllerPickerView::worthAsking(const std::vector<PadDescription>& pads)
{
    if (pads.size() < 2)
        return false;

    for (std::size_t i = 1; i < pads.size(); i++) {
        if (Differ(pads[0], pads[i]))
            return true;
    }
    return false;
}

ControllerPickerView::ControllerPickerView(std::vector<PadDescription> pads,
                                          OnChosen onChosen, Describe describe)
    : m_pads(std::move(pads))
    , m_onChosen(std::move(onChosen))
    , m_describe(std::move(describe))
{
    const auto& p = app::views::active();

    setAxis(brls::Axis::COLUMN);
    setJustifyContent(brls::JustifyContent::CENTER);
    setAlignItems(brls::AlignItems::CENTER);
    setGrow(1.0f);
    setWidthPercentage(100.0f);
    setHeightPercentage(100.0f);
    setBackgroundColor(nvgRGBA(0, 0, 0, 0));

    m_panel = new brls::Box();
    m_panel->setAxis(brls::Axis::COLUMN);
    m_panel->setWidth(1120);
    m_panel->setHeight(514);
    m_panel->setPaddingTop(25);
    m_panel->setPaddingBottom(18);
    m_panel->setPaddingLeft(28);
    m_panel->setPaddingRight(28);
    m_panel->setCornerRadius(20);
    m_panel->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
    addView(m_panel);

    auto* title = new brls::Label();
    title->setText("Choose a controller");
    title->setFontSize(28);
    title->setTextColor(p.text);
    title->setMarginBottom(3);
    m_panel->addView(title);

    auto* prompt = new brls::Box();
    prompt->setAxis(brls::Axis::ROW);
    prompt->setJustifyContent(brls::JustifyContent::FLEX_START);
    prompt->setAlignItems(brls::AlignItems::CENTER);
    prompt->setMarginBottom(15);
    m_panel->addView(prompt);

    auto* before = new brls::Label();
    before->setText("Press");
    before->setFontSize(17);
    before->setTextColor(p.textMuted);
    prompt->addView(before);

    auto* lBtn = new brls::Image();
    lBtn->setImageFromRes("img/buttons/l.png");
    lBtn->setWidth(44);
    lBtn->setHeight(32);
    lBtn->setMarginLeft(8);
    prompt->addView(lBtn);

    auto* plus = new brls::Label();
    plus->setText("+");
    plus->setFontSize(16);
    plus->setTextColor(p.textDim);
    plus->setMarginLeft(4);
    plus->setMarginRight(4);
    prompt->addView(plus);

    auto* rBtn = new brls::Image();
    rBtn->setImageFromRes("img/buttons/r.png");
    rBtn->setWidth(44);
    rBtn->setHeight(32);
    rBtn->setMarginRight(8);
    prompt->addView(rBtn);

    auto* after = new brls::Label();
    after->setText("on the controller you want to use");
    after->setFontSize(17);
    after->setTextColor(p.textMuted);
    prompt->addView(after);

    auto* rule = new brls::Box();
    rule->setWidthPercentage(100.0f);
    rule->setHeight(1);
    rule->setBackgroundColor(p.surfaceLine);
    rule->setMarginBottom(17);
    m_panel->addView(rule);

    m_row = new brls::Box();
    m_row->setAxis(brls::Axis::ROW);
    m_row->setJustifyContent(brls::JustifyContent::CENTER);
    m_row->setAlignItems(brls::AlignItems::CENTER);
    m_row->setHeight(274);
    m_panel->addView(m_row);

    buildCards();

    auto* hintBar = new brls::Box();
    hintBar->setAxis(brls::Axis::ROW);
    hintBar->setJustifyContent(brls::JustifyContent::FLEX_END);
    hintBar->setAlignItems(brls::AlignItems::CENTER);
    hintBar->setWidthPercentage(100.0f);
    hintBar->setPaddingTop(8);
    hintBar->setMarginTop(13);
    hintBar->setBorderColor(p.surfaceLine);
    hintBar->setBorderThickness(1.0f);

    auto* hints = new brls::Hints();
    hints->setHintFontSizes(22.0f, 16.0f);
    hintBar->addView(hints);
    m_panel->addView(hintBar);

    setFocusable(true);

    registerAction("Use controller", brls::ControllerButton::BUTTON_A,
        [this](brls::View*) {
            accept();
            return true;
        });

    setActionAvailable(brls::ControllerButton::BUTTON_A, false);

    registerAction("Back", brls::ControllerButton::BUTTON_B,
        [this](brls::View*) {
            cancel();
            return true;
        });
}

void ControllerPickerView::accept()
{
    if (m_done)
        return;

    if (m_selected >= 0 && m_selected < (int)m_pads.size()) {
        log::info("ControllerPicker: chose {} on npad {}",
            m_pads[m_selected].label, (int)m_pads[m_selected].npad);
        choose((std::size_t)m_selected);
        return;
    }

    log::info("ControllerPicker: continuing on the default pad");
    m_done = true;
    if (m_onChosen)
        m_onChosen(kNoChoice);
}

void ControllerPickerView::cancel()
{
    if (m_done)
        return;

    log::info("ControllerPicker: backed out");
    m_done = true;
    if (m_onChosen)
        m_onChosen(kCancelled);
}

void ControllerPickerView::buildCards()
{
    const auto& p = app::views::active();

    m_states.resize(m_pads.size());
    m_seen_clear.assign(m_pads.size(), false);

    constexpr float kCanvas   = 1040.0f;
    constexpr float kGutter   = 20.0f;
    constexpr float kMaxCard  = 300.0f;
    constexpr float kMinCard  = 148.0f;

    const float count    = (float)(m_pads.size() > 0 ? m_pads.size() : 1);
    const float budget   = (kCanvas - 60.0f) / count - kGutter;
    const float cardW    = budget > kMaxCard ? kMaxCard : (budget < kMinCard ? kMinCard : budget);
    const float scale    = cardW / kMaxCard;

    const float artW     = scale < 0.72f ? 84.0f : 112.0f;
    const float artH     = scale < 0.72f ? 84.0f : 112.0f;

    for (std::size_t i = 0; i < m_pads.size(); i++) {
        padInitialize(&m_states[i], m_pads[i].npad);
        padUpdate(&m_states[i]);

        auto* card = new brls::Box();
        card->setAxis(brls::Axis::COLUMN);
        card->setAlignItems(brls::AlignItems::CENTER);
        card->setJustifyContent(brls::JustifyContent::CENTER);
        card->setWidth(cardW);
        card->setHeight(252);
        card->setMarginLeft(10);
        card->setMarginRight(10);
        card->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        card->setCornerRadius(18);

        auto* art = new brls::Image();
        art->setImageFromRes(PadArtPath(m_pads[i]));
        art->setScalingType(brls::ImageScalingType::FIT);
        art->setWidth(artW);
        art->setHeight(artH);
        art->setMarginBottom(8);
        card->addView(art);

        auto* name = new brls::Label();
        name->setText(m_pads[i].label);
        name->setFontSize(scale < 0.8f ? 18 : 21);
        name->setTextColor(p.text);
        name->setMarginBottom(2);
        card->addView(name);

        auto* slot = new brls::Label();
        slot->setText(SlotLine(m_pads[i].npad));
        slot->setFontSize(14);
        slot->setTextColor(p.textMuted);
        slot->setMarginBottom(9);
        card->addView(slot);

        auto* chips = new brls::Box();
        chips->setAxis(brls::Axis::ROW);
        chips->setJustifyContent(brls::JustifyContent::CENTER);
        chips->setAlignItems(brls::AlignItems::CENTER);
        chips->setMarginBottom(8);
        for (const auto& capability : CapabilityLabels(m_pads[i]))
            chips->addView(MakeCapabilityChip(capability));
        card->addView(chips);

        if (m_pads[i].caps.battery) {
            const bool split = PadHasTwoBatteries(m_pads[i]);
            card->addView(new BatteryPips(m_pads[i].npad, split, split ? 78 : 44, 18));
        }

        m_cards.push_back(card);
        m_row->addView(card);
    }
}

bool ControllerPickerView::SameSet(const std::vector<PadDescription>& a,
                                   const std::vector<PadDescription>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i].npad != b[i].npad || a[i].kind != b[i].kind)
            return false;
    }
    return true;
}

void ControllerPickerView::rebuild(std::vector<PadDescription> fresh)
{
    bool         hadSelection = m_selected >= 0 && m_selected < (int)m_pads.size();
    PadPathKind  selKind      = hadSelection ? m_pads[m_selected].kind : PadPathKind::JoyCon;
    HidNpadIdType selNpad     = hadSelection ? m_pads[m_selected].npad : HidNpadIdType_No1;
    uint16_t     selVid       = hadSelection ? m_pads[m_selected].vendor_id  : 0;
    uint16_t     selPid       = hadSelection ? m_pads[m_selected].product_id : 0;

    m_pads = std::move(fresh);
    m_cards.clear();
    m_row->clearViews();
    buildCards();

    m_selected = carryOverSelection(hadSelection, selNpad, selKind, selVid, selPid);
    setActionAvailable(brls::ControllerButton::BUTTON_A, m_selected >= 0);
}

int ControllerPickerView::carryOverSelection(bool hadSelection, HidNpadIdType selNpad,
                                             PadPathKind selKind, uint16_t selVid,
                                             uint16_t selPid) const
{
    if (!hadSelection)
        return -1;

    for (std::size_t i = 0; i < m_pads.size(); i++) {
        if (m_pads[i].npad == selNpad && m_pads[i].kind == selKind)
            return (int)i;
    }

    if (selVid != 0) {
        for (std::size_t i = 0; i < m_pads.size(); i++) {
            if (m_pads[i].vendor_id == selVid && m_pads[i].product_id == selPid)
                return (int)i;
        }
    }

    if (selKind == PadPathKind::JoyCon) {
        int count = 0, only = -1;
        for (std::size_t i = 0; i < m_pads.size(); i++) {
            if (m_pads[i].kind == PadPathKind::JoyCon) { count++; only = (int)i; }
        }
        if (count == 1)
            return only;
    }

    return -1;
}

void ControllerPickerView::choose(std::size_t index)
{
    if (m_done || index >= m_pads.size())
        return;

    m_done = true;
    if (m_onChosen)
        m_onChosen(m_pads[index].npad);
}

void ControllerPickerView::draw(NVGcontext* vg, float x, float y, float width, float height,
                                brls::Style style, brls::FrameContext* ctx)
{
    const auto& p = app::views::active();

    NVGpaint scrim = nvgLinearGradient(vg, x, y, x, y + height,
        app::views::withAlpha(p.backgroundDeep, 0x8c),
        app::views::withAlpha(p.gradientBottom, 0xd4));
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, scrim);
    nvgFill(vg);

    if (m_panel && m_panel->getWidth() > 0.0f && m_panel->getHeight() > 0.0f)
        app::views::drawGlassSurface(vg, m_panel->getX(), m_panel->getY(),
            m_panel->getWidth(), m_panel->getHeight(), 20.0f, 1.0f, false, p);

    for (std::size_t i = 0; i < m_cards.size(); i++) {
        auto* card = m_cards[i];
        if (!card || card->getWidth() <= 0.0f || card->getHeight() <= 0.0f)
            continue;
        app::views::drawGlassSurface(vg, card->getX(), card->getY(),
            card->getWidth(), card->getHeight(), 18.0f, 0.92f,
            (int)i == m_selected, p);
    }

    Box::draw(vg, x, y, width, height, style, ctx);

    if (m_selected >= 0 && m_selected < (int)m_cards.size()) {
        brls::Box* card = m_cards[m_selected];
        const float cx = card->getX(), cy = card->getY();
        const float cw = card->getWidth(), ch = card->getHeight();

        for (int pass = 3; pass >= 0; pass--) {
            const float grow  = 3.0f + pass * 4.0f;
            const int   alpha = pass == 0 ? 230 : 58 - pass * 10;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - grow, cy - grow, cw + grow * 2, ch + grow * 2, 18.0f + grow);
            nvgStrokeColor(vg, app::views::withAlpha(
                pass == 0 ? p.accent : p.focusA, (unsigned char)alpha));
            nvgStrokeWidth(vg, pass == 0 ? 2.5f : 5.0f);
            nvgStroke(vg);
        }
    }

    if (m_done)
        return;

    m_frames++;

    if (m_describe && (m_frames % 30) == 0) {
        auto fresh = m_describe();
        if (!SameSet(fresh, m_pads)) {
            log::info("ControllerPicker: pads changed, {} -> {}",
                               m_pads.size(), fresh.size());
            rebuild(std::move(fresh));
            return;
        }
    }

    if (m_frames > 1800) {
        log::warning("ControllerPicker: no input in ~30s, continuing");
        accept();
        return;
    }

    for (std::size_t i = 0; i < m_states.size(); i++) {
        padUpdate(&m_states[i]);

        const u64 held = padGetButtons(&m_states[i]);

        if ((m_frames % 120) == 1) {
            log::info("ControllerPicker: npad {} connected={} held=0x{:x}",
                               (int)m_pads[i].npad, padIsConnected(&m_states[i]) ? 1 : 0,
                               (unsigned long long)held);
        }

        constexpr u64 kShoulder = HidNpadButton_L  | HidNpadButton_R;
        constexpr u64 kTrigger   = HidNpadButton_ZL | HidNpadButton_ZR;

        const bool picking = ((held & kShoulder) == kShoulder) ||
                             ((held & kTrigger)  == kTrigger);

        if (!picking) {
            m_seen_clear[i] = true;
            continue;
        }

        if (!m_seen_clear[i])
            continue;

        if (m_selected != (int)i) {
            m_selected = (int)i;
            setActionAvailable(brls::ControllerButton::BUTTON_A, true);
            log::info("ControllerPicker: selected {} on npad {}",
                m_pads[i].label, (int)m_pads[i].npad);
        }
        return;
    }
}

}

