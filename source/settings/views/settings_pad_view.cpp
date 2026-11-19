#include "settings/views/settings_pad_view.hpp"
#include "app/view_services.hpp"

#include <vector>

#include "platform/factory.hpp"
#include "settings/views/lightbar_picker.hpp"

#include <borealis/core/i18n.hpp>

namespace akira::settings::views {

using namespace brls::literals;

brls::Box* SettingsPadView::makeCard()
{
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setWidth(brls::View::AUTO);
    card->setHeight(brls::View::AUTO);
    card->setGrow(1.0f);
    card->setCornerRadius(14.0f);
    card->setBackgroundColor(brls::Application::getTheme()["color/card"]);
    card->setMarginBottom(30.0f);
    return card;
}

brls::Label* SettingsPadView::makeHeading(const std::string& text)
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(15.0f);
    label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    label->setMargins(14.0f, 15.0f, 6.0f, 15.0f);
    return label;
}

SettingsPadView::SettingsPadView(const akira::platform::PadDescription& pad)
    : m_pad(pad)
{
    services = &app::ViewServices::current();

    m_platform = app::ViewServices::current().platform.createPadSettings(pad);
    m_key = m_platform->rumbleKey(services->controllers);

    services->controllers.seedRumbleProfile(m_key);
    m_profile = services->controllers.rumbleProfile(m_key);
    m_platform->useProfile(m_profile);

    setAxis(brls::Axis::COLUMN);
    setWidth(brls::View::AUTO);
    setHeight(brls::View::AUTO);
    setGrow(1.0f);
    setPadding(30.0f, 40.0f, 30.0f, 40.0f);
    setBackgroundColor(brls::Application::getTheme()["brls/background"]);

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
                         [](brls::View*) {
                             brls::Application::popActivity();
                             return true;
                         }, true);

    buildHeader();

    auto* scroll = new brls::ScrollingFrame();
    scroll->setWidth(brls::View::AUTO);
    scroll->setHeight(brls::View::AUTO);
    scroll->setGrow(1.0f);

    content = new brls::Box(brls::Axis::COLUMN);
    content->setWidth(brls::View::AUTO);
    content->setHeight(brls::View::AUTO);
    scroll->setContentView(content);
    addView(scroll);

    const bool direct = m_platform->directOutput();
    if (direct)
        buildOutputRows();

    if (!direct)
        buildSourceRows();

    buildIntensityRow();

    if (m_platform->tunesMotors()) {
        buildStrengthRows();

        if (!m_profile.per_motor && !direct)
            buildFrequencyRows();
    }

    if (!direct)
        buildEnvelopeRows();

    if (direct)
        buildLightbarRows();

    if (m_pad.kind == akira::platform::PadPathKind::JoyCon)
        buildGyroRow();

    buildActionRows();
}

void SettingsPadView::buildHeader()
{
    auto* title = new brls::Label();
    title->setText(m_pad.label);
    title->setFontSize(30.0f);
    addView(title);

    std::string tier;
    if (m_key == play::kRumbleKeySwitch)
        tier = "akira/settings/rumble_tier_switch"_i18n;
    else if (m_key == play::kRumbleKeyDefault)
        tier = "akira/settings/rumble_tier_default"_i18n;
    else if (m_platform->isUnitKey(m_key))
        tier = "akira/settings/rumble_tier_unit"_i18n;
    else
        tier = m_key;

    auto* sub = new brls::Label();
    sub->setText(tier);
    sub->setFontSize(16.0f);
    sub->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    sub->setMarginBottom(20.0f);
    addView(sub);
}

void SettingsPadView::buildOutputRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_output_heading"_i18n));

    if (m_platform->outputReleased()) {
        auto* row = new brls::DetailCell();
        row->setMarginLeft(15.0f);
        row->setMarginRight(15.0f);
        row->title->setText("akira/settings/rumble_output_mode"_i18n);
        row->setDetailText("akira/settings/rumble_output_basic"_i18n);
        row->setDetailTextColor(
            brls::Application::getTheme()["brls/text_disabled"]);
        card->addView(row);

        auto* why = makeHeading("akira/settings/rumble_output_released_hint"_i18n);
        why->setMarginLeft(15.0f);
        why->setMarginRight(15.0f);
        why->setMarginBottom(6.0f);
        card->addView(why);

        content->addView(card);
        return;
    }

    auto* mode = new brls::SelectorCell();
    mode->setMarginLeft(15.0f);
    mode->setMarginRight(15.0f);

    const std::vector<std::string> options = {
        "akira/settings/rumble_output_native"_i18n,
        "akira/settings/rumble_output_basic"_i18n,
    };

    mode->init("akira/settings/rumble_output_mode"_i18n, options,
               static_cast<int>(m_profile.output_mode),
               [this](int selected) {
                   const auto chosen = selected == 1
                                           ? play::PadOutputMode::Basic
                                           : play::PadOutputMode::Native;
                   if (chosen == m_profile.output_mode)
                       return;

                   m_profile.output_mode = chosen;
                   store();

                   m_platform->refreshOutputGates();

                   reopen();
               });

    mode->setDetailText(m_profile.output_mode == play::PadOutputMode::Basic
                            ? "akira/settings/rumble_output_basic_hint"_i18n
                            : "akira/settings/rumble_output_native_hint"_i18n);
    card->addView(mode);

    content->addView(card);
}

void SettingsPadView::buildLightbarRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/lightbar_heading"_i18n));

    auto* on = new brls::BooleanCell();
    on->setMarginLeft(15.0f);
    on->setMarginRight(15.0f);
    on->init("akira/settings/lightbar_enabled"_i18n, m_profile.lightbar_enabled,
             [this](bool value) {
                 m_profile.lightbar_enabled = value;
                 store();
                 reopen();
             });
    card->addView(on);

    if (!m_profile.lightbar_enabled) {
        content->addView(card);
        return;
    }

    auto* picker = new LightbarPicker(m_profile.lightbar_r, m_profile.lightbar_g,
                                      m_profile.lightbar_b);
    picker->setMarginLeft(15.0f);
    picker->setMarginRight(15.0f);
    picker->setMarginTop(4.0f);

    auto* swatch = new brls::Box();
    swatch->setHeight(30.0f);
    swatch->setMarginLeft(15.0f);
    swatch->setMarginRight(15.0f);
    swatch->setMarginTop(10.0f);
    swatch->setMarginBottom(4.0f);
    swatch->setCornerRadius(6.0f);
    swatch->setBackgroundColor(
        nvgRGB(m_profile.lightbar_r, m_profile.lightbar_g, m_profile.lightbar_b));

    picker->setChangeListener([this, swatch](std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        m_profile.lightbar_r = r;
        m_profile.lightbar_g = g;
        m_profile.lightbar_b = b;
        swatch->setBackgroundColor(nvgRGB(r, g, b));
        store();
    });

    card->addView(picker);
    card->addView(swatch);

    auto* how = makeHeading(m_platform->drivesLightbar()
                                ? "akira/settings/lightbar_hint"_i18n
                                : "akira/settings/lightbar_idle"_i18n);
    how->setMarginLeft(15.0f);
    how->setMarginRight(15.0f);
    how->setMarginBottom(6.0f);
    card->addView(how);

    content->addView(card);
}

void SettingsPadView::buildSourceRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_source_heading"_i18n));

    auto* source = new brls::SelectorCell();
    source->setMarginLeft(15.0f);
    source->setMarginRight(15.0f);

    const std::vector<std::string> options = {
        "akira/settings/rumble_source_off"_i18n,
        "akira/settings/rumble_source_derived"_i18n,
        "akira/settings/rumble_source_game"_i18n,
    };

    const auto hintFor = [](play::RumbleSource value) {
        switch (value) {
            case play::RumbleSource::Off:
                return "akira/settings/rumble_source_hint_off"_i18n;
            case play::RumbleSource::Game:
                return "akira/settings/rumble_source_hint_game"_i18n;
            default:
                return "akira/settings/rumble_source_hint_derived"_i18n;
        }
    };

    source->init("akira/settings/rumble_source"_i18n, options,
                 static_cast<int>(m_profile.rumble_source),
                 [this, source, hintFor](int selected) {
                     m_profile.rumble_source =
                         static_cast<play::RumbleSource>(selected);
                     store();
                     source->setDetailText(hintFor(m_profile.rumble_source));
                 });

    source->setDetailText(hintFor(m_profile.rumble_source));
    card->addView(source);

    content->addView(card);
}

void SettingsPadView::buildEnvelopeRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_envelope_heading"_i18n));

    auto* attack = new brls::SliderCell();
    attack->setMarginLeft(15.0f);
    attack->setMarginRight(15.0f);
    attack->slider->setDiscreteStep(0.01f / 0.80f);
    attack->init("akira/settings/rumble_attack"_i18n,
                 (m_profile.envelope_attack - 0.20f) / 0.80f,
                 [this, attack](float value) {
                     m_profile.envelope_attack = (float)(int)((0.20f + value * 0.80f) * 100.0f) / 100.0f;
                     attack->setDetailText(brls::getStr("akira/settings/percent_format",
                                                        (int)(m_profile.envelope_attack * 100.0f)));
                     store();
                 });
    attack->setDetailText(brls::getStr("akira/settings/percent_format",
                                       (int)(m_profile.envelope_attack * 100.0f)));
    card->addView(attack);

    auto* sustain = new brls::SliderCell();
    sustain->setMarginLeft(15.0f);
    sustain->setMarginRight(15.0f);
    sustain->slider->setDiscreteStep(0.01f / 0.45f);
    sustain->init("akira/settings/rumble_sustain"_i18n,
                  (m_profile.envelope_decay - 0.50f) / 0.45f,
                  [this, sustain](float value) {
                      m_profile.envelope_decay = (float)(int)((0.50f + value * 0.45f) * 100.0f) / 100.0f;
                      sustain->setDetailText(brls::getStr("akira/settings/percent_format",
                                                          (int)(m_profile.envelope_decay * 100.0f)));
                      store();
                  });
    sustain->setDetailText(brls::getStr("akira/settings/percent_format",
                                        (int)(m_profile.envelope_decay * 100.0f)));
    card->addView(sustain);

    content->addView(card);
}

void SettingsPadView::buildIntensityRow()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_intensity_heading"_i18n));

    auto* intensity = new brls::SelectorCell();
    intensity->setMarginLeft(15.0f);
    intensity->setMarginRight(15.0f);

    const std::vector<std::string> options = {
        "akira/settings/haptic_intensity_off"_i18n,
        "akira/settings/haptic_intensity_very_weak"_i18n,
        "akira/settings/haptic_intensity_weak"_i18n,
        "akira/settings/haptic_intensity_normal"_i18n,
        "akira/settings/haptic_intensity_strong"_i18n,
        "akira/settings/haptic_intensity_very_strong"_i18n,
    };

    intensity->init("akira/settings/haptic_intensity"_i18n, options,
                    static_cast<int>(m_profile.haptic_intensity),
                    [this](int selected) {
                        m_profile.haptic_intensity =
                            static_cast<play::HapticIntensity>(selected);
                        store();
                    });

    card->addView(intensity);
    content->addView(card);
}

void SettingsPadView::buildStrengthRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_strength_heading"_i18n));

    auto* strength = new brls::SliderCell();
    strength->setMarginLeft(15.0f);
    strength->setMarginRight(15.0f);
    strength->init("akira/settings/rumble_strength"_i18n, m_profile.strength,
                   [this, strength](float value) {
                       m_profile.strength = value;
                       strength->setDetailText(std::to_string(value).substr(0, 4));
                       store();
                   });
    strength->setDetailText(std::to_string(m_profile.strength).substr(0, 4));
    card->addView(strength);

    auto* ceiling = new brls::SliderCell();
    ceiling->setMarginLeft(15.0f);
    ceiling->setMarginRight(15.0f);
    ceiling->init("akira/settings/rumble_ceiling"_i18n, m_profile.ceiling,
                  [this, ceiling](float value) {
                      m_profile.ceiling = value;
                      ceiling->setDetailText(std::to_string(value).substr(0, 4));
                      store();
                  });
    ceiling->setDetailText(std::to_string(m_profile.ceiling).substr(0, 4));
    card->addView(ceiling);

    content->addView(card);
}

void SettingsPadView::buildFrequencyRows()
{
    auto* card = makeCard();
    card->addView(makeHeading("akira/settings/rumble_frequency_heading"_i18n));

    auto* low = new brls::SliderCell();
    low->setMarginLeft(15.0f);
    low->setMarginRight(15.0f);
    low->slider->setDiscreteStep(5.0f / 280.0f);
    low->init("akira/settings/rumble_low_freq"_i18n,
              (m_profile.freq_low - 40.0f) / 280.0f,
              [this, low](float value) {
                  m_profile.freq_low = (float)(int)(40.0f + value * 280.0f);
                  low->setDetailText(brls::getStr("akira/settings/hz_format",
                                                  (int)m_profile.freq_low));
                  store();
              });
    low->setDetailText(brls::getStr("akira/settings/hz_format", (int)m_profile.freq_low));
    card->addView(low);

    auto* high = new brls::SliderCell();
    high->setMarginLeft(15.0f);
    high->setMarginRight(15.0f);
    high->slider->setDiscreteStep(5.0f / 280.0f);
    high->init("akira/settings/rumble_high_freq"_i18n,
               (m_profile.freq_high - 40.0f) / 280.0f,
               [this, high](float value) {
                   m_profile.freq_high = (float)(int)(40.0f + value * 280.0f);
                   high->setDetailText(brls::getStr("akira/settings/hz_format",
                                                    (int)m_profile.freq_high));
                   store();
               });
    high->setDetailText(brls::getStr("akira/settings/hz_format", (int)m_profile.freq_high));
    card->addView(high);

    content->addView(card);
}

void SettingsPadView::buildGyroRow()
{
    auto* card = makeCard();

    auto* gyro = new brls::SelectorCell();
    gyro->setMarginLeft(15.0f);
    gyro->setMarginRight(15.0f);

    const std::vector<std::string> options = {
        "akira/settings/gyro_auto"_i18n,
        "akira/settings/gyro_left"_i18n,
        "akira/settings/gyro_right"_i18n,
    };

    gyro->init("akira/settings/gyro_source"_i18n, options,
               static_cast<int>(services->controllers.gyroSource()),
               [this](int selected) {
                   services->controllers.setGyroSource(static_cast<play::GyroSource>(selected));
                   services->store.save();
               });

    card->addView(gyro);
    content->addView(card);
}

void SettingsPadView::buildActionRows()
{
    auto* card = makeCard();

    auto* buzz = new brls::DetailCell();
    buzz->setMarginLeft(15.0f);
    buzz->setMarginRight(15.0f);
    buzz->setText("akira/settings/rumble_test"_i18n);
    buzz->setDetailText("akira/settings/rumble_test_hint"_i18n);
    buzz->registerClickAction([this](brls::View*) {
        previewBuzz();
        return true;
    });
    card->addView(buzz);

    auto* reset = new brls::DetailCell();
    reset->setMarginLeft(15.0f);
    reset->setMarginRight(15.0f);
    reset->setText("akira/settings/rumble_reset"_i18n);
    reset->setDetailText("akira/settings/rumble_reset_hint"_i18n);
    reset->registerClickAction([this](brls::View*) {
        services->controllers.resetRumbleProfile(m_key);
        m_profile = services->controllers.rumbleProfile(m_key);
        services->store.save();

        reopen();
        return true;
    });
    card->addView(reset);

    content->addView(card);
}

void SettingsPadView::store()
{
    services->controllers.setRumbleProfile(m_key, m_profile);
    services->store.save();
}

static void ReopenWhenSettled(const akira::platform::PadDescription& pad, int attempts_left)
{
    if (brls::Application::isInputBlocks() && attempts_left > 0) {
        brls::delay(16, [pad, attempts_left]() { ReopenWhenSettled(pad, attempts_left - 1); });
        return;
    }

    brls::Application::popActivity(brls::TransitionAnimation::NONE, [pad]() {
        brls::Application::pushActivity(new brls::Activity(new SettingsPadView(pad)),
                                        brls::TransitionAnimation::NONE);
    });
}

void SettingsPadView::reopen()
{
    brls::delay(1, [pad = m_pad]() { ReopenWhenSettled(pad, 60); });
}

void SettingsPadView::previewBuzz()
{
    if (auto preview = m_platform->previewDirect(m_profile)) {
        if (preview->stop)
            brls::delay(300, preview->stop);
        return;
    }

    auto* inputMgr = brls::Application::getPlatform()->getInputManager();
    const float amplitude = m_profile.strength * m_profile.ceiling;

    inputMgr->sendRumbleToNpad((unsigned int)m_pad.npad,
                               m_profile.freq_low, m_profile.freq_high,
                               amplitude, amplitude);

    brls::delay(300, [inputMgr, npad = m_pad.npad]() {
        inputMgr->sendRumbleToNpad((unsigned int)npad, 0.0f, 0.0f, 0.0f, 0.0f);
    });
}

}
