#include "input/pad_path_mc.hpp"

#include <algorithm>
#include <cstring>

#include "input/extended_input_manager.hpp"
#include "input/ps_output.hpp"
#include "core/settings_manager.hpp"

#include <borealis.hpp>

namespace akira::input {

namespace {

struct ButtonMap {
    uint32_t ps;
    uint32_t button;
};

constexpr ButtonMap kButtons[] = {
    { PsButton_Cross,    static_cast<uint32_t>(libchiaking::controllers::ControllerButton::cross) },
    { PsButton_Circle,   static_cast<uint32_t>(libchiaking::controllers::ControllerButton::moon) },
    { PsButton_Square,   static_cast<uint32_t>(libchiaking::controllers::ControllerButton::box) },
    { PsButton_Triangle, static_cast<uint32_t>(libchiaking::controllers::ControllerButton::pyramid) },
    { PsButton_Left,     static_cast<uint32_t>(libchiaking::controllers::ControllerButton::dpad_left) },
    { PsButton_Right,    static_cast<uint32_t>(libchiaking::controllers::ControllerButton::dpad_right) },
    { PsButton_Up,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::dpad_up) },
    { PsButton_Down,     static_cast<uint32_t>(libchiaking::controllers::ControllerButton::dpad_down) },
    { PsButton_L1,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::l1) },
    { PsButton_R1,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::r1) },
    { PsButton_L3,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::l3) },
    { PsButton_R3,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::r3) },
    { PsButton_Options,  static_cast<uint32_t>(libchiaking::controllers::ControllerButton::options) },
    { PsButton_Create,   static_cast<uint32_t>(libchiaking::controllers::ControllerButton::share) },
    { PsButton_Touchpad, static_cast<uint32_t>(libchiaking::controllers::ControllerButton::touchpad) },
    { PsButton_Ps,       static_cast<uint32_t>(libchiaking::controllers::ControllerButton::ps) },
};

} // namespace

McGenericPath::McGenericPath(HidNpadIdType npad, ExtendedInputManager& extended,
                             uint16_t vendor_id, uint16_t product_id)
    : HosPadPath(npad, HidNpadStyleTag_NpadFullKey)
    , m_extended(extended)
    , m_vendor_id(vendor_id)
    , m_product_id(product_id)
{
    if (auto* input = brls::Application::getPlatform()->getInputManager())
        input->refreshRumbleHandles((unsigned int)npad);
}

PadCapabilities McGenericPath::capabilities() const
{
    PadCapabilities caps;
    caps.gyro            = true;
    caps.rumble          = true;
    caps.analog_triggers = false;
    return caps;
}

bool McGenericPath::poll()
{
    return HosPadPath::poll();
}

void McGenericPath::sendRumble(float left, float right, float freqLow, float freqHigh)
{
    const float lo = std::clamp(left,  0.0f, 1.0f);
    const float hi = std::clamp(right, 0.0f, 1.0f);

    if (lo > 0.0f || hi > 0.0f) {
        const uint32_t now =
            (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000000ull);
        if (now >= m_rumble_log_next) {
            m_rumble_log_next = now + 1000;
            brls::Logger::info("generic rumble: npad {} lo={:.3f} hi={:.3f}",
                               (int)m_npad, lo, hi);
        }
    }

    auto* inputMgr = brls::Application::getPlatform()->getInputManager();
    inputMgr->sendRumbleToNpad((unsigned int)m_npad, freqLow, freqHigh, lo, hi);
}

void McGenericPath::readTriggers(libchiaking::controllers::ControllerState& state)
{
    if (!m_extended.hasFreshAnalogState())
        return;

    state.l2 = m_extended.l2();
    state.r2 = m_extended.r2();
}

McPsNativePath::McPsNativePath(HidNpadIdType npad, ExtendedInputManager& extended,
                               const PsModel& model, const uint8_t* bt_addr)
    : McGenericPath(npad, extended, model.vendor_id, model.product_id)
    , m_model(&model)
{
    if (bt_addr != nullptr) {
        std::memcpy(m_address, bt_addr, sizeof(m_address));
        m_have_address = true;
    }

    m_label = model.name;
    m_extended.setRawWanted(true);

    m_home_blocked = R_SUCCEEDED(appletBeginBlockingHomeButton(0));

    m_direct_output = PadTakesDirectOutput(model.vendor_id, model.product_id);
    m_extended.setDirectOutput(m_direct_output);

    brls::Logger::info("McPsNativePath: home blocking short={} direct rumble={}",
                       m_home_blocked, m_direct_output);
}

McPsNativePath::~McPsNativePath()
{
    if (m_home_blocked)
        appletEndBlockingHomeButton();

    if (m_direct_output) {
        m_extended.setDirectRumble(0, 0);
        m_extended.setDirectOutput(false);
    }
}

PadCapabilities McPsNativePath::capabilities() const
{
    PadCapabilities caps = McGenericPath::capabilities();
    caps.touchpad = m_model->has_touchpad;

    caps.analog_triggers = true;
    return caps;
}

bool McPsNativePath::poll()
{
    const bool connected = McGenericPath::poll();

    AkiraInputRawReport raw{};
    m_report_valid = false;

    const bool got_raw = m_have_address
        ? m_extended.readRawReportFor(m_address, &raw)
        : m_extended.readRawReport(&raw);

    if (got_raw && raw.vendor_id == m_vendor_id && raw.product_id == m_product_id) {
        m_report_valid = ParsePsReport(*m_model, raw.data, raw.length, &m_report);

        if (!m_have_address) {
            std::memcpy(m_address, raw.bt_addr, sizeof(m_address));
            m_have_address = true;
        }
    }

    return connected;
}

void McPsNativePath::readButtons(libchiaking::controllers::ControllerState& state)
{
    if (!m_report_valid)
        return;

    auto* settings = SettingsManager::getInstance();

    for (const ButtonMap& m : kButtons) {
        if (!(m_report.buttons & m.ps))
            continue;
        if (!settings->isButtonEnabled(m.button))
            continue;
        state.buttons |= m.button;
    }
}

void McPsNativePath::sendRumble(float left, float right, float freqLow, float freqHigh)
{
    const bool direct = m_direct_output && ExtendedInputManager::directStreamAllowed();

    if (!m_rumble_source_logged && (left > 0.0f || right > 0.0f)) {
        m_rumble_source_logged = true;
        brls::Logger::info("rumble source: {}",
                           direct ? "DIRECT - written to the pad by us"
                                  : (m_direct_output
                                        ? "MissionControl - direct path present but the stream toggle is off"
                                        : "MissionControl - this pad is not driven directly"));
    }

    if (!direct) {
        McGenericPath::sendRumble(left, right, freqLow, freqHigh);
        return;
    }

    const float lo = std::clamp(left,  0.0f, 1.0f);
    const float hi = std::clamp(right, 0.0f, 1.0f);

    m_extended.setDirectRumble((uint8_t)(lo * 255.0f + 0.5f),
                               (uint8_t)(hi * 255.0f + 0.5f));
}

void McPsNativePath::sendTriggerEffects(const TriggerEffect& left, const TriggerEffect& right)
{
    if (!m_direct_output)
        return;

    m_extended.setDirectTriggerEffects(left.type, left.params, right.type, right.params);
}

void McPsNativePath::sendEffectIntensity(uint8_t vibration, uint8_t trigger)
{
    if (!m_direct_output)
        return;

    m_extended.setConsoleIntensity(akira::input::Ds5IntensityFromWire(vibration),
                                   akira::input::Ds5IntensityFromWire(trigger));
}

void McPsNativePath::sendLightbar(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!m_direct_output)
        return;

    m_extended.setGameLightbar(red, green, blue);
}

bool McPsNativePath::menuHeld() const
{
    if (!m_report_valid)
        return McGenericPath::menuHeld();

    return (m_report.buttons & PsButton_Options) != 0;
}

void McPsNativePath::readTriggers(libchiaking::controllers::ControllerState& state)
{
    if (!m_report_valid) {
        McGenericPath::readTriggers(state);
        return;
    }

    state.l2 = m_report.l2;
    state.r2 = m_report.r2;
}

void McPsNativePath::releaseSlot(libchiaking::controllers::ControllerInput& input, TouchSlot& slot)
{
    if (!slot.active)
        return;

    if (slot.touch_id >= 0)
        input.stop_touch(static_cast<uint8_t>(slot.touch_id));

    slot.active    = false;
    slot.touch_id = -1;
}

bool McPsNativePath::readTouchpad(libchiaking::controllers::ControllerInput& input)
{
    if (!m_model->has_touchpad)
        return false;

    if (!m_report_valid || !m_report.touch_fresh)
        return true;

    for (int i = 0; i < 2; i++) {
        const PsTouchPoint& point = m_report.touch[i];
        TouchSlot& slot = m_slots[i];

        if (!point.down) {
            releaseSlot(input, slot);
            continue;
        }

        if (slot.active && slot.pad_id != point.id)
            releaseSlot(input, slot);

        if (!slot.active) {
            const auto id = input.start_touch(point.x, point.y);
            if (!id)
                continue;

            slot.active    = true;
            slot.pad_id    = point.id;
            slot.touch_id = static_cast<int8_t>(*id);
            continue;
        }

        input.move_touch(static_cast<uint8_t>(slot.touch_id), point.x, point.y);
    }

    return true;
}

} // namespace akira::input
