#include "stream/input_manager.hpp"
#include "input/sample_cadence.hpp"
#include "stream/session.hpp"
#include "platform/video/renderer.hpp"
#include "core/settings_manager.hpp"
#include "core/swipe_direction.hpp"
#include "input/remap_target.hpp"
#include "platform/factory.hpp"
#include <borealis.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>

InputManager::InputManager()
    : m_pads(akira::platform::PlatformFactory::instance().pads())
    , m_state_mutex(m_pads.mutex())
{
}

InputManager::~InputManager()
{
    cleanup();
}

std::uint32_t InputManager::button(libchiaking::controllers::ControllerButton held) noexcept
{
    return static_cast<std::uint32_t>(held);
}

int8_t InputManager::startTouch(libchiaking::controllers::ControllerInput& input, uint16_t x, uint16_t y) noexcept
{
    const auto started = input.start_touch(x, y);
    return started ? static_cast<int8_t>(*started) : int8_t{-1};
}

void InputManager::setTargetPS5(bool ps5)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_is_ps5 = ps5;
}

bool InputManager::init()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    bool vibration_permitted = true;
    if (R_SUCCEEDED(hidIsVibrationPermitted(&vibration_permitted)) && !vibration_permitted) {
        brls::Logger::warning("Controller Vibration is off in System Settings - "
                              "nothing sent through HOS will be felt");
    }

    for (int i = 0; i < SDL_JOYSTICK_COUNT; i++)
    {
        m_sdl_joystick_ptr[i] = SDL_JoystickOpen(i);
        if (m_sdl_joystick_ptr[i] == nullptr)
        {
            brls::Logger::error("SDL_JoystickOpen: {}", SDL_GetError());
            return false;
        }
    }

    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    hidInitializeTouchScreen();

    m_pads.open();

    return true;
}

void InputManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_menu_held.store(false, std::memory_order_release);

    for (int i = 0; i < SDL_JOYSTICK_COUNT; i++)
    {
        if (m_sdl_joystick_ptr[i])
        {
            SDL_JoystickClose(m_sdl_joystick_ptr[i]);
            m_sdl_joystick_ptr[i] = nullptr;
        }
    }

    m_pads.close();
    m_overlay_drag_finger = -1;
}


void InputManager::sendRumble(float left, float right, float freqLow, float freqHigh,
    float nonNativeScale)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    akira::input::Pad* pad = m_pads.current();
    if (!pad)
        return;
    if (!pad->nativeRumble()) {
        left *= nonNativeScale;
        right *= nonNativeScale;
    }
    pad->sendRumble(left, right, freqLow, freqHigh);
}


void InputManager::applyPadInput(akira::input::Pad& pad, libchiaking::controllers::ControllerState& state)
{
    state.buttons = 0;
    state.l2 = 0x00;
    state.r2 = 0x00;

    const bool mapped = pad.usesButtonMapping();

    u64 buttons = mapped ? pad.heldButtons() : 0;
    u64 consumedButtons = 0;

    if (!mapped) {
        pad.readButtons(state);
        pad.readTriggers(state);
    } else {
    const ButtonMapping& mapping = SettingsManager::getInstance()->getButtonMapping();
    for (const auto& [chiakiBtn, combo] : mapping) {
        if (combo.size() <= 1) continue;
        if (!SettingsManager::getInstance()->isButtonEnabled(chiakiBtn))
            continue;
        bool allHeld = true;
        for (uint64_t hidBtn : combo) {
            if (!(buttons & hidBtn)) { allHeld = false; break; }
        }
        if (allHeld) {
            for (uint64_t hidBtn : combo)
                consumedButtons |= hidBtn;
        }
    }
    static constexpr uint32_t swipeConstants[4] = {
        SWIPE_TOUCHPAD_UP, SWIPE_TOUCHPAD_DOWN,
        SWIPE_TOUCHPAD_LEFT, SWIPE_TOUCHPAD_RIGHT
    };
    for (int i = 0; i < 4; i++) {
        if (!SettingsManager::getInstance()->isButtonEnabled(swipeConstants[i]))
            continue;
        if (m_swipes[i].phase == SyntheticSwipe::Phase::ACTIVE) {
            auto it = mapping.find(swipeConstants[i]);
            if (it != mapping.end()) {
                for (uint64_t hidBtn : it->second)
                    consumedButtons |= hidBtn;
            }
        }
    }

    u64 dpadButtons = buttons & ~consumedButtons;
    if (dpadButtons & HidNpadButton_Left)  state.buttons |= button(libchiaking::controllers::ControllerButton::dpad_left);
    if (dpadButtons & HidNpadButton_Right) state.buttons |= button(libchiaking::controllers::ControllerButton::dpad_right);
    if (dpadButtons & HidNpadButton_Up)    state.buttons |= button(libchiaking::controllers::ControllerButton::dpad_up);
    if (dpadButtons & HidNpadButton_Down)  state.buttons |= button(libchiaking::controllers::ControllerButton::dpad_down);

    for (const auto& [chiakiBtn, combo] : mapping) {
        if (combo.empty()) continue;
        if (chiakiBtn & 0xFF000000) continue;
        if (!SettingsManager::getInstance()->isButtonEnabled(chiakiBtn))
            continue;

        if (combo.size() == 1 && (consumedButtons & combo[0]))
            continue;

        bool allHeld = true;
        for (uint64_t hidBtn : combo) {
            if (!(buttons & hidBtn)) { allHeld = false; break; }
        }

        if (allHeld) {
            if (chiakiBtn == akira::input::remapKey(akira::input::RemapTarget::L2)) {
                state.l2 = 0xff;
            } else if (chiakiBtn == akira::input::remapKey(akira::input::RemapTarget::R2)) {
                state.r2 = 0xff;
            } else {
                state.buttons |= chiakiBtn;
            }
        }
    }

    pad.readTriggers(state);
    }

    HidAnalogStickState left = pad.stickPos(0);
    HidAnalogStickState right = pad.stickPos(1);

    static constexpr u64 leftStickDirs = HidNpadButton_StickLUp | HidNpadButton_StickLDown
                                       | HidNpadButton_StickLLeft | HidNpadButton_StickLRight;
    static constexpr u64 rightStickDirs = HidNpadButton_StickRUp | HidNpadButton_StickRDown
                                        | HidNpadButton_StickRLeft | HidNpadButton_StickRRight;

    if (consumedButtons & leftStickDirs) {
        state.left_x = 0;
        state.left_y = 0;
    } else {
        state.left_x = left.x;
        state.left_y = -left.y;
    }

    if (consumedButtons & rightStickDirs) {
        state.right_x = 0;
        state.right_y = 0;
    } else {
        state.right_x = right.x;
        state.right_y = -right.y;
    }
}


void InputManager::update(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id)
{
    akira::platform::PadProvider::Callbacks callbacks;
    std::unique_lock<std::mutex> lock(m_state_mutex);

    akira::input::Pad* pad = m_pads.acquire(callbacks);
    if (!pad)
    {
        input.set_idle();
        lock.unlock();
        for (auto& callback : callbacks)
            callback();
        return;
    }

    const auto now = akira::input::gesture::Clock::now();

    pad->poll();
    m_menu_held.store(pad->menuHeld(), std::memory_order_release);

    applyPadInput(*pad, input.state());

    u64 buttons = pad->usesButtonMapping() ? pad->heldButtons() : 0;

    if (!pad->readTouchpad(input))
        updateTouchInput(input, finger_id_touch_id, buttons, now);

    if (m_motion_cadence.due(now))
        readSixAxis(input.state());

    lock.unlock();
    for (auto& callback : callbacks)
        callback();
}

void InputManager::updateTouchInput(libchiaking::controllers::ControllerInput& input,
    std::map<uint32_t, int8_t>* finger_id_touch_id, u64 buttons,
    akira::input::gesture::TimePoint now)
{
    readTouchScreen(input, finger_id_touch_id, now);
    updateSyntheticSwipes(input, buttons, now);

    if (m_touchpad_button_pulse.scheduled)
    {
        if (now >= m_touchpad_button_pulse.releaseAt)
        {
            m_touchpad_button_pulse.scheduled = false;
            fireDeferredRelease(input);
        }
        else if (now >= m_touchpad_button_pulse.pressAt)
        {
            input.state().buttons |= button(libchiaking::controllers::ControllerButton::touchpad);
        }
    }
    else
    {
        fireDeferredRelease(input);
    }
}

void InputManager::fireDeferredRelease(libchiaking::controllers::ControllerInput& input)
{
    if (m_deferred_release_touch_id < 0)
        return;
    brls::Logger::info("Touch: deferred stop_touch={} fires now", m_deferred_release_touch_id);
    input.stop_touch(static_cast<uint8_t>(m_deferred_release_touch_id));
    m_deferred_release_touch_id = -1;
    m_active_click_touch_id = -1;
}

bool InputManager::readTouchScreen(libchiaking::controllers::ControllerInput& input,
    std::map<uint32_t, int8_t>* finger_id_touch_id,
    akira::input::gesture::TimePoint now)
{
    HidTouchScreenState sw_state = {0};

    size_t got = hidGetTouchScreenStates(&sw_state, 1);
    if (got == 0)
    {
        if (!finger_id_touch_id->empty() && now >= m_next_touch_warning)
        {
            brls::Logger::warning("Touch: hidGetTouchScreenStates returned 0, preserving {} active touches", finger_id_touch_id->size());
            m_next_touch_warning = now + akira::input::gesture::TouchWarningInterval;
        }
        return !finger_id_touch_id->empty();
    }

    if (sw_state.count > 1)
    {
        s32 target = -1;
        for (s32 i = 0; i < sw_state.count; i++)
        {
            uint32_t fid = sw_state.touches[i].finger_id;
            if (finger_id_touch_id->count(fid) || m_pending_border_taps.count(fid))
            {
                target = i;
                break;
            }
        }
        if (target < 0)
            target = 0;
        if (target != 0)
            sw_state.touches[0] = sw_state.touches[target];
        sw_state.count = 1;
    }

    if (sw_state.count > 0 && m_prev_touch_count == 0)
        brls::Logger::info("Touch: started, {} point(s), finger_id={}, pos=({},{})",
            sw_state.count, sw_state.touches[0].finger_id, sw_state.touches[0].x, sw_state.touches[0].y);
    else if (sw_state.count == 0 && m_prev_touch_count > 0)
        brls::Logger::info("Touch: released, had {} tracked finger(s)", finger_id_touch_id->size());
    m_prev_touch_count = sw_state.count;

    bool ret = false;

    for (auto it = finger_id_touch_id->begin(); it != finger_id_touch_id->end();)
    {
        auto cur = it;
        it++;
        bool found = false;
        for (int i = 0; i < sw_state.count; i++)
        {
            if (sw_state.touches[i].finger_id == cur->first)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (cur->second >= 0)
            {
                if (m_touchpad_button_pulse.scheduled)
                {
                    m_deferred_release_touch_id = cur->second;
                    brls::Logger::debug("Touch: defer stop_touch={} until button hold completes", cur->second);
                }
                else
                {
                    brls::Logger::debug("Touch: stop touch_id={} for finger_id={}", cur->second, cur->first);
                    input.stop_touch(static_cast<uint8_t>(cur->second));
                }
            }
            finger_id_touch_id->erase(cur);
        }
    }

    for (auto pt = m_pending_border_taps.begin(); pt != m_pending_border_taps.end();)
    {
        bool found = false;
        for (int i = 0; i < sw_state.count; i++)
        {
            if (sw_state.touches[i].finger_id == pt->first)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            brls::Logger::info("Touch: pending border tap for finger_id={} lifted before commit", pt->first);
            pt = m_pending_border_taps.erase(pt);
        }
        else
        {
            ++pt;
        }
    }

    const float trackpadMaxX = m_is_ps5 ? 1919.0f : 1920.0f;
    const float trackpadMaxY = m_is_ps5 ? 1079.0f : 942.0f;

    akira::platform::VideoRenderer* renderer = Session::GetInstance()->getVideoRenderer();
    if (m_overlay_drag_finger >= 0)
    {
        bool still_down = false;
        for (int i = 0; i < sw_state.count; i++)
        {
            if ((int64_t)sw_state.touches[i].finger_id == m_overlay_drag_finger)
            {
                still_down = true;
                break;
            }
        }
        if (!still_down)
        {
            if (renderer)
                renderer->overlayTouchEnd();
            m_overlay_drag_finger = -1;
        }
    }

    const float screenScaleX = (float)brls::Application::windowWidth / (float)SWITCH_TOUCHSCREEN_MAX_X;
    const float screenScaleY = (float)brls::Application::windowHeight / (float)SWITCH_TOUCHSCREEN_MAX_Y;

    for (int i = 0; i < sw_state.count; i++)
    {
        uint16_t x = sw_state.touches[i].x * (trackpadMaxX / (float)SWITCH_TOUCHSCREEN_MAX_X);
        uint16_t y = sw_state.touches[i].y * (trackpadMaxY / (float)SWITCH_TOUCHSCREEN_MAX_Y);

        uint32_t rawX = sw_state.touches[i].x;
        uint32_t rawY = sw_state.touches[i].y;
        bool isBorder = rawX <= 64 || rawX >= (SWITCH_TOUCHSCREEN_MAX_X - 64) ||
                        rawY <= 64 || rawY >= (SWITCH_TOUCHSCREEN_MAX_Y - 64);

        auto it = finger_id_touch_id->find(sw_state.touches[i].finger_id);
        bool isTracked = it != finger_id_touch_id->end();

        auto pt = m_pending_border_taps.find(sw_state.touches[i].finger_id);
        bool isPending = pt != m_pending_border_taps.end();

        if ((int64_t)sw_state.touches[i].finger_id == m_overlay_drag_finger)
        {
            if (renderer)
                renderer->overlayTouchMove(rawX * screenScaleX, rawY * screenScaleY);
            ret = true;
            continue;
        }

        if (!isTracked && !isPending)
        {
            if (renderer && renderer->overlayTouchBegin(rawX * screenScaleX, rawY * screenScaleY))
            {
                m_overlay_drag_finger = (int64_t)sw_state.touches[i].finger_id;
                ret = true;
                continue;
            }

            if (isBorder)
            {
                m_pending_border_taps[sw_state.touches[i].finger_id] =
                    {(uint16_t)rawX, (uint16_t)rawY, now};
                brls::Logger::info("Touch: pending border tap for finger_id={} raw=({},{})",
                    sw_state.touches[i].finger_id, rawX, rawY);
            }
            else
            {
                fireDeferredRelease(input);
                const int8_t touch_id = startTouch(input, x, y);
                (*finger_id_touch_id)[sw_state.touches[i].finger_id] = touch_id;
                brls::Logger::info("Touch: new finger_id={} -> touch_id={}, raw=({},{}) mapped=({},{})",
                    sw_state.touches[i].finger_id, touch_id,
                    sw_state.touches[i].x, sw_state.touches[i].y, x, y);
                if (touch_id < 0)
                    brls::Logger::warning("Touch: no free touch slots (max {})", libchiaking::controllers::ControllerState{}.touches.size());
            }
        }
        else if (isPending)
        {
            int dx = (int)rawX - (int)pt->second.down_x;
            int dy = (int)rawY - (int)pt->second.down_y;
            bool moved = dx*dx + dy*dy > PendingBorderTap::TAP_MAX_MOVE * PendingBorderTap::TAP_MAX_MOVE;

            if (moved)
            {
                fireDeferredRelease(input);
                const int8_t touch_id = startTouch(input, x, y);
                (*finger_id_touch_id)[sw_state.touches[i].finger_id] = touch_id;
                brls::Logger::info("Touch: border swipe committed finger_id={} -> touch_id={} at mapped=({},{})",
                    sw_state.touches[i].finger_id, touch_id, x, y);
                m_pending_border_taps.erase(pt);
            }
            else if (now - pt->second.downAt >= akira::input::gesture::BorderTapCommitDelay)
            {
                fireDeferredRelease(input);
                const int8_t touch_id = startTouch(input, x, y);
                (*finger_id_touch_id)[sw_state.touches[i].finger_id] = touch_id;
                m_touchpad_button_pulse.scheduled = true;
                m_touchpad_button_pulse.pressAt = now + akira::input::gesture::TouchpadButtonDelay;
                m_touchpad_button_pulse.releaseAt = m_touchpad_button_pulse.pressAt
                    + akira::input::gesture::TouchpadButtonHold;
                m_active_click_touch_id = touch_id;
                Session::GetInstance()->triggerBorderFlash();
                brls::Logger::info("Touch: border tap committed (touch first, button in 67 ms, pos frozen) finger_id={} -> touch_id={} at mapped=({},{})",
                    sw_state.touches[i].finger_id, touch_id, x, y);
                m_pending_border_taps.erase(pt);
            }
        }
        else if (it->second >= 0)
        {
            if (it->second != m_active_click_touch_id)
            {
                input.move_touch(static_cast<uint8_t>(it->second), x, y);
                brls::Logger::debug("Touch: move touch_id={} mapped=({},{})", it->second, x, y);
            }
        }
        ret = true;
    }
    return ret;
}

bool InputManager::readSixAxis(libchiaking::controllers::ControllerState& state)
{
    akira::input::Pad* pad = m_pads.current();
    if (!pad)
        return false;
    auto reading = pad->readGyro();
    if (!reading)
        return false;
    HidSixAxisSensorState& sixaxis = *reading;

    state.gyro_x = sixaxis.angular_velocity.x * 2.0f * M_PI;
    state.gyro_y = sixaxis.angular_velocity.z * 2.0f * M_PI;
    state.gyro_z = -sixaxis.angular_velocity.y * 2.0f * M_PI;

    m_raw_accel_x = -sixaxis.acceleration.x;
    m_raw_accel_y = -sixaxis.acceleration.z;
    m_raw_accel_z = sixaxis.acceleration.y;

    state.accel_x = m_raw_accel_x - m_accel_zero_x;
    state.accel_y = m_raw_accel_y - m_accel_zero_y;
    state.accel_z = m_raw_accel_z - m_accel_zero_z;

    // Convert rotation matrix to quaternion
    float (*dm)[3] = sixaxis.direction.direction;
    float m[3][3] = {
        {dm[0][0], dm[2][0], dm[1][0]},
        {dm[0][2], dm[2][2], dm[1][2]},
        {dm[0][1], dm[2][1], dm[1][1]}
    };
    std::array<float, 4> q;
    float t;
    if (m[2][2] < 0)
    {
        if (m[0][0] > m[1][1])
        {
            t = 1 + m[0][0] - m[1][1] - m[2][2];
            q = {t, m[0][1] + m[1][0], m[2][0] + m[0][2], m[1][2] - m[2][1]};
        }
        else
        {
            t = 1 - m[0][0] + m[1][1] - m[2][2];
            q = {m[0][1] + m[1][0], t, m[1][2] + m[2][1], m[2][0] - m[0][2]};
        }
    }
    else
    {
        if (m[0][0] < -m[1][1])
        {
            t = 1 - m[0][0] - m[1][1] + m[2][2];
            q = {m[2][0] + m[0][2], m[1][2] + m[2][1], t, m[0][1] - m[1][0]};
        }
        else
        {
            t = 1 + m[0][0] + m[1][1] + m[2][2];
            q = {m[1][2] - m[2][1], m[2][0] - m[0][2], m[0][1] - m[1][0], t};
        }
    }
    float fac = 0.5f / sqrt(t);
    state.orient_x = q[0] * fac;
    state.orient_y = q[1] * fac;
    state.orient_z = -q[2] * fac;
    state.orient_w = q[3] * fac;
    return true;
}

void InputManager::resetMotionControls()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    akira::input::Pad* pad = m_pads.current();
    if (!pad)
        return;

    m_accel_zero_x = m_raw_accel_x;
    m_accel_zero_y = m_raw_accel_y - 1.0f;
    m_accel_zero_z = m_raw_accel_z;

    pad->resetMotion();

    brls::Logger::info("Motion controls reset: zero offset = ({}, {}, {})",
        m_accel_zero_x, m_accel_zero_y, m_accel_zero_z);
}

void InputManager::updateSyntheticSwipes(libchiaking::controllers::ControllerInput& state, u64 buttons,
    akira::input::gesture::TimePoint now)
{
    static constexpr uint32_t swipeConstants[4] = {
        SWIPE_TOUCHPAD_UP, SWIPE_TOUCHPAD_DOWN,
        SWIPE_TOUCHPAD_LEFT, SWIPE_TOUCHPAD_RIGHT
    };

    const int16_t padMaxX = m_is_ps5 ? 1919 : 1920;
    const int16_t padMaxY = m_is_ps5 ? 1079 : 942;

    struct SwipeConfig {
        int16_t startX, startY;
        int16_t endX, endY;
    };
    SwipeConfig configs[4] = {
        {(int16_t)(padMaxX / 2), padMaxY, (int16_t)(padMaxX / 2), 0},
        {(int16_t)(padMaxX / 2), 0, (int16_t)(padMaxX / 2), padMaxY},
        {padMaxX, (int16_t)(padMaxY / 2), 0, (int16_t)(padMaxY / 2)},
        {0, (int16_t)(padMaxY / 2), padMaxX, (int16_t)(padMaxY / 2)},
    };

    const ButtonMapping& mapping = SettingsManager::getInstance()->getButtonMapping();

    akira::input::Pad* pad = m_pads.current();
    if (!pad)
        return;
    HidAnalogStickState leftStick = pad->stickPos(0);
    HidAnalogStickState rightStick = pad->stickPos(1);

    static const char* swipeNames[4] = { "UP", "DOWN", "LEFT", "RIGHT" };

    for (int i = 0; i < 4; i++) {
        SyntheticSwipe& swipe = m_swipes[i];

        if (!SettingsManager::getInstance()->isButtonEnabled(swipeConstants[i])) {
            swipe.buttonWasPressed = false;
            if (swipe.phase == SyntheticSwipe::Phase::ACTIVE) {
                brls::Logger::info("Swipe {}: disabled mid-swipe, stopping touch_id={}", swipeNames[i], swipe.touchId);
                state.stop_touch(static_cast<uint8_t>(swipe.touchId));
                swipe.phase = SyntheticSwipe::Phase::IDLE;
                swipe.touchId = -1;
            }
            continue;
        }

        auto it = mapping.find(swipeConstants[i]);
        if (it == mapping.end() || it->second.empty()) {
            swipe.buttonWasPressed = false;
            continue;
        }

        bool comboHeld = true;
        for (uint64_t hidBtn : it->second) {
            if (!(buttons & hidBtn)) {
                comboHeld = false;
                break;
            }
        }

        bool stickDirectionValid = true;
        if (comboHeld) {
            for (uint64_t hidBtn : it->second) {
                if (hidBtn == HidNpadButton_StickRUp || hidBtn == HidNpadButton_StickRDown) {
                    if (abs(rightStick.x) > abs(rightStick.y)) { stickDirectionValid = false; break; }
                } else if (hidBtn == HidNpadButton_StickRLeft || hidBtn == HidNpadButton_StickRRight) {
                    if (abs(rightStick.y) > abs(rightStick.x)) { stickDirectionValid = false; break; }
                } else if (hidBtn == HidNpadButton_StickLUp || hidBtn == HidNpadButton_StickLDown) {
                    if (abs(leftStick.x) > abs(leftStick.y)) { stickDirectionValid = false; break; }
                } else if (hidBtn == HidNpadButton_StickLLeft || hidBtn == HidNpadButton_StickLRight) {
                    if (abs(leftStick.y) > abs(leftStick.x)) { stickDirectionValid = false; break; }
                }
            }
        }

        if (comboHeld && !stickDirectionValid)
            brls::Logger::debug("Swipe {}: combo held but stick direction invalid (L={},{} R={},{})",
                swipeNames[i], leftStick.x, leftStick.y, rightStick.x, rightStick.y);

        if (swipe.phase == SyntheticSwipe::Phase::IDLE) {
            if (comboHeld && stickDirectionValid && !swipe.buttonWasPressed) {
                swipe.startX = configs[i].startX;
                swipe.startY = configs[i].startY;
                swipe.endX = configs[i].endX;
                swipe.endY = configs[i].endY;
                swipe.touchId = startTouch(state, swipe.startX, swipe.startY);
                if (swipe.touchId >= 0) {
                    swipe.phase = SyntheticSwipe::Phase::ACTIVE;
                    swipe.startedAt = now;
                    brls::Logger::info("Swipe {}: started touch_id={}, start=({},{}) end=({},{}) duration=300 ms",
                        swipeNames[i], swipe.touchId, swipe.startX, swipe.startY,
                        swipe.endX, swipe.endY);
                } else {
                    brls::Logger::warning("Swipe {}: no free touch slots", swipeNames[i]);
                }
            }
            swipe.buttonWasPressed = comboHeld && stickDirectionValid;
        } else {
            const auto elapsed = now - swipe.startedAt;
            if (elapsed >= akira::input::gesture::SwipeDuration) {
                brls::Logger::info("Swipe {}: completed, final pos=({},{}), touch_id={}",
                    swipeNames[i], swipe.endX, swipe.endY, swipe.touchId);
                state.stop_touch(static_cast<uint8_t>(swipe.touchId));
                swipe.phase = SyntheticSwipe::Phase::IDLE;
                swipe.touchId = -1;
                swipe.buttonWasPressed = comboHeld && stickDirectionValid;
            } else {
                const int16_t x = akira::input::gesture::interpolate(
                    swipe.startX, swipe.endX, elapsed, akira::input::gesture::SwipeDuration);
                const int16_t y = akira::input::gesture::interpolate(
                    swipe.startY, swipe.endY, elapsed, akira::input::gesture::SwipeDuration);
                state.move_touch(static_cast<uint8_t>(swipe.touchId), x, y);
            }
        }
    }
}
