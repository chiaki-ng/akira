#ifndef AKIRA_IO_INPUT_MANAGER_HPP
#define AKIRA_IO_INPUT_MANAGER_HPP

#include <SDL2/SDL.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <switch.h>

#include "input/extended_input_manager.hpp"
#include "input/controller_timing.hpp"
#include "input/gesture_timing.hpp"
#include "input/pad_path.hpp"
#include "input/sample_cadence.hpp"

#include <memory>
#include <span>
#include <vector>

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

struct PadPathInfo {
    bool available = false;
    uint64_t generation = 0;
    akira::input::PadPathKind kind = akira::input::PadPathKind::JoyCon;
    HidNpadIdType npad = HidNpadIdType_No1;
    HidNpadStyleTag style = HidNpadStyleTag_NpadFullKey;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::array<uint8_t, 6> address{};
    bool hasAddress = false;
    bool nativeRumble = false;
    bool switchNative = true;
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

    void readSecondaryPads(std::span<libchiaking::controllers::ControllerInput> states, uint8_t& count);

    static constexpr uint8_t kNoCouchSlot = 0xff;

    uint8_t couchJoin(HidNpadIdType npad);
    void couchLeave(HidNpadIdType npad);
    std::map<HidNpadIdType, uint8_t> couchRoster() const;
    uint8_t couchPadCount() const;

    uint8_t CouchPadIndexForController(HidNpadIdType npad);

    bool couchSlotOutputTarget(uint8_t slot, uint8_t out_addr[6], uint16_t* vendor_id, uint16_t* product_id);

    void beginCouchJoin();
    void cancelCouchJoin();
    void tickCouchClaim();
    bool couchClaimActive() const;
    float couchClaimProgress() const;
    void setOnCouchJoined(std::function<void(HidNpadIdType, uint8_t)> cb);
    void setOnCouchDisconnected(std::function<void(HidNpadIdType, uint8_t)> cb);
    void setOnPadArrived(std::function<void(HidNpadIdType)> cb);
    void promptControllerSetup();
    bool isClaimableNpad(HidNpadIdType npad) const;
    HidNpadIdType boundNpad() const;
    void resolvePadArrival();

    void setPath(std::unique_ptr<akira::input::PadPath> path);
    PadPathInfo pathInfo() const;
    void sendRumble(float left, float right, float freqLow, float freqHigh,
        float nonNativeScale = 1.0f);
    void sendTriggerEffects(const akira::input::PadPath::TriggerEffect& left,
        const akira::input::PadPath::TriggerEffect& right);
    void sendEffectIntensity(uint8_t vibration, uint8_t trigger);
    void sendLightbar(uint8_t red, uint8_t green, uint8_t blue);

    void selectNpad(HidNpadIdType npad);

    std::vector<akira::input::PadDescription> describePads();

    ExtendedInputManager& extendedInput() { return m_extended; }
    bool menuHeld() const { return m_menu_held.load(std::memory_order_acquire); }

private:
    using DeferredCallbacks = std::vector<std::function<void()>>;

    void setPathLocked(std::unique_ptr<akira::input::PadPath> path);
    PadPathInfo pathInfoLocked() const;
    std::vector<akira::input::PadDescription> describePadsLocked();
    uint8_t couchJoinLocked(HidNpadIdType npad);
    void couchLeaveLocked(HidNpadIdType npad);
    uint8_t couchPadCountLocked() const;
    bool isClaimableNpadLocked(HidNpadIdType npad) const;

    void retryPathIdentification();

    void reconcilePathDriver();

    void pollCouchClaim(DeferredCallbacks& callbacks);

    void pollPadArrivals(DeferredCallbacks& callbacks);

    uint64_t couchNpadMask() const;

    static std::uint32_t button(libchiaking::controllers::ControllerButton held) noexcept;
    static int8_t startTouch(libchiaking::controllers::ControllerInput& input, uint16_t x, uint16_t y) noexcept;

    void applyPadInput(akira::input::PadPath* path, libchiaking::controllers::ControllerState& state);

    bool readTouchScreen(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id,
        akira::input::gesture::TimePoint now);
    bool readSixAxis(libchiaking::controllers::ControllerState& state);
    void updateSyntheticSwipes(libchiaking::controllers::ControllerInput& state, u64 buttons,
        akira::input::gesture::TimePoint now);

    bool m_is_ps5 = false;
    SDL_Joystick* m_sdl_joystick_ptr[SDL_JOYSTICK_COUNT] = {nullptr};

    ExtendedInputManager m_extended;

    mutable std::mutex m_state_mutex;
    std::unique_ptr<akira::input::PadPath> m_path;
    uint64_t m_path_generation = 0;
    std::atomic<bool> m_menu_held{false};

    static constexpr HidNpadIdType kNoNpad = (HidNpadIdType)0xff;
    HidNpadIdType m_bound_npad     = kNoNpad;
    std::map<HidNpadIdType, uint8_t> m_couch_roster;
    std::map<HidNpadIdType, std::unique_ptr<akira::input::PadPath>> m_secondary_paths;
    std::map<HidNpadIdType, akira::input::timing::TimePoint> m_secondary_missing_since;
    std::vector<akira::input::PadDescription> m_couch_pads;
    akira::input::timing::TimePoint m_couch_pads_read{};
    bool m_couch_claim_active = false;
    std::map<HidNpadIdType, akira::input::timing::TimePoint> m_claim_started_at;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_joined;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_disconnected;
    std::function<void(HidNpadIdType)> m_on_pad_arrived;
    uint32_t m_connected_mask = 0;
    uint32_t m_announced_mask = 0;
    bool     m_arrival_primed = false;
    bool     m_arrival_pending = false;
    akira::input::SampleCadence m_arrival_cadence{akira::input::timing::ArrivalPollPeriod};
    bool          m_identify_done  = false;
    uint32_t      m_identify_next  = 0;
    uint32_t      m_identify_until = 0;
    uint32_t      m_driver_next    = 0;
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
