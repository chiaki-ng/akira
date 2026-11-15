#ifndef AKIRA_IO_INPUT_MANAGER_HPP
#define AKIRA_IO_INPUT_MANAGER_HPP

#include <SDL2/SDL.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <switch.h>

#include "input/gesture_timing.hpp"
#include "input/pad.hpp"
#include "input/sample_cadence.hpp"
#include "platform/pad_source.hpp"


#include <libchiaking/play_types.hpp>

#define SDL_JOYSTICK_COUNT 2

// Trackpad and touchscreen dimensions for coordinate mapping
#define DS4_TRACKPAD_MAX_X 1920
#define DS4_TRACKPAD_MAX_Y 942
#define SWITCH_TOUCHSCREEN_MAX_X 1280
#define SWITCH_TOUCHSCREEN_MAX_Y 720

struct SyntheticSwipe {
    enum class Phase { IDLE, ACTIVE };
    Phase phase = Phase::IDLE;
    int8_t touchId = -1;
    int16_t startX = 0;
    int16_t startY = 0;
    int16_t endX = 0;
    int16_t endY = 0;
    akira::input::gesture::TimePoint startedAt{};
    bool buttonWasPressed = false;
};

struct PendingBorderTap {
    uint16_t down_x;
    uint16_t down_y;
    akira::input::gesture::TimePoint downAt;
    static constexpr int TAP_MAX_MOVE = 80;
};

struct TouchpadButtonPulse {
    bool scheduled = false;
    akira::input::gesture::TimePoint pressAt{};
    akira::input::gesture::TimePoint releaseAt{};
};


class InputManager
{
public:
    InputManager();
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void setTargetPS5(bool ps5);

    bool init();
    void cleanup();
    void update(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id);

    void sendRumble(float left, float right, float freqLow, float freqHigh,
        float nonNativeScale = 1.0f);
    bool menuHeld() const { return m_menu_held.load(std::memory_order_acquire); }

    void applyPadInput(akira::input::Pad& pad, libchiaking::controllers::ControllerState& state);

private:
    static std::uint32_t button(libchiaking::controllers::ControllerButton held) noexcept;
    static int8_t startTouch(libchiaking::controllers::ControllerInput& input, uint16_t x, uint16_t y) noexcept;

    bool readTouchScreen(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id,
        akira::input::gesture::TimePoint now);
    bool readSixAxis(libchiaking::controllers::ControllerState& state);
    void updateSyntheticSwipes(libchiaking::controllers::ControllerInput& state, u64 buttons,
        akira::input::gesture::TimePoint now);
    void updateTouchInput(libchiaking::controllers::ControllerInput& input,
        std::map<uint32_t, int8_t>* finger_id_touch_id, u64 buttons,
        akira::input::gesture::TimePoint now);

    bool m_is_ps5 = false;
    SDL_Joystick* m_sdl_joystick_ptr[SDL_JOYSTICK_COUNT] = {nullptr};

    akira::platform::PadSource& m_pads;
    std::mutex& m_state_mutex;
    std::atomic<bool> m_menu_held{false};

    SyntheticSwipe m_swipes[4];

    std::map<uint32_t, PendingBorderTap> m_pending_border_taps;
    TouchpadButtonPulse m_touchpad_button_pulse;
    void fireDeferredRelease(libchiaking::controllers::ControllerInput& input);

    int64_t m_overlay_drag_finger = -1;
    int8_t m_deferred_release_touch_id = -1;
    int8_t m_active_click_touch_id = -1;

    int m_prev_touch_count = 0;
    akira::input::gesture::TimePoint m_next_touch_warning{};
    akira::input::SampleCadence m_motion_cadence{akira::input::MotionPollPeriod};

    // Accelerometer zero offset for gyro reset
    float m_accel_zero_x = 0.0f;
    float m_accel_zero_y = 0.0f;
    float m_accel_zero_z = 0.0f;

    // Current raw accel values (before offset applied)
    float m_raw_accel_x = 0.0f;
    float m_raw_accel_y = 0.0f;
    float m_raw_accel_z = 0.0f;

public:
    void resetMotionControls();
};

#endif // AKIRA_IO_INPUT_MANAGER_HPP
