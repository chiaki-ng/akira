#ifndef AKIRA_INPUT_EXTENDED_INPUT_MANAGER_HPP
#define AKIRA_INPUT_EXTENDED_INPUT_MANAGER_HPP

#include <atomic>
#include <cstdint>
#include <string>
#include <switch.h>

#include <libchiaking/play_types.hpp>
#include <libchiaking/platform/platform.hpp>

#include "akira_input/ipc.h"
#include "input/pad_output_state.hpp"
#include "input/ps_output.hpp"

class ExtendedInputManager
{
public:
    enum class Availability {
        NotInstalled,
        Unsupported,
        VersionMismatch,
        Available,
        Failed,
    };

    ExtendedInputManager() = default;
    ~ExtendedInputManager();

    ExtendedInputManager(const ExtendedInputManager&) = delete;
    ExtendedInputManager& operator=(const ExtendedInputManager&) = delete;

    static Availability probe(uint32_t* out_mc_version);

    static const char* describe(Availability availability);

    static std::string describeActiveDevice();

    static bool listDevicesOnce(AkiraInputDeviceList* out);

    static bool writeOutputReportOnce(const uint8_t* address, const uint8_t* data, uint16_t length);

    static void setDirectStreamAllowed(bool allowed);
    static bool directStreamAllowed();

    akira::input::PadOutputState outputState() const;

    static void refreshDirectGates(uint16_t vendor_id, uint16_t product_id,
                                   const uint8_t* address);

    static bool outputOwnershipRefused()
    {
        return s_output_refused.load(std::memory_order_acquire);
    }

    static bool backendDirectOutputEnabled();

    bool backendReleased() const
    {
        return s_backend_released.load(std::memory_order_acquire);
    }
    static void setDirectHapticsAllowed(bool allowed);
    static bool directHapticsAllowed();

    static bool probeAudio(const uint8_t* address, AkiraInputAudioProbe* out);
    static bool readRawReportOnce(AkiraInputRawReport* out);
    static std::string describeDirectState();

    static bool probeDirectWrite(const uint8_t* address, uint8_t method,
                                 const uint8_t* frame, uint16_t length,
                                 uint32_t* out_rc, uint32_t* out_init_rc);

private:
    static void loadDirectGatesOnce();
    static std::atomic<bool> s_direct_stream_allowed;
    static std::atomic<bool> s_direct_haptics_allowed;


    bool                     m_direct_gate_logged = false;

public:

    bool initializeOptional();
    void shutdown();

    Availability availability() const { return m_availability; }
    uint32_t missionControlVersion() const { return m_mc_version; }

    bool consumeDegradedNotice();

    bool hasFreshAnalogState() const;
    uint8_t l2() const;
    uint8_t r2() const;

    bool readRawReport(AkiraInputRawReport* out) const;
    bool readRawReportFor(const uint8_t* bt_addr, AkiraInputRawReport* out) const;

    bool listDevices(AkiraInputDeviceList* out) const;

    void setRawWanted(bool wanted);

    void setDirectOutput(bool enabled);

    void resolveDirectAddress();

    void ensureOutputOwnership(bool want);

    void paintLightbar();

    bool resolveLightbar(uint8_t* rgb) const;

    void setGameLightbar(uint8_t red, uint8_t green, uint8_t blue);
    void clearGameLightbar();

    void setConsoleIntensity(akira::input::Ds5EffectIntensity vibration,
                             akira::input::Ds5EffectIntensity trigger);
    akira::input::Ds5EffectIntensity consoleVibrationIntensity() const;
    akira::input::Ds5EffectIntensity consoleTriggerIntensity() const;

    static std::atomic<uint32_t> s_direct_vid_pid;

    static std::atomic<bool> s_backend_released;

    void setDirectRumble(uint8_t left, uint8_t right);

    void setDirectTriggerEffects(uint8_t left_type, const uint8_t* left_params,
                                 uint8_t right_type, const uint8_t* right_params);

    bool sendDirectHaptics(uint8_t* frame, uint16_t length);

    bool directHapticsReady() const;

    void registerCouchOutput(const uint8_t* bt_addr, uint16_t vendor_id, uint16_t product_id);
    void unregisterCouchOutput(const uint8_t* bt_addr);
    void setCouchRumble(const uint8_t* bt_addr, uint8_t left, uint8_t right);
    void setCouchTriggerEffects(const uint8_t* bt_addr,
                                uint8_t left_type, const uint8_t* left_params,
                                uint8_t right_type, const uint8_t* right_params);
    void setCouchLightbar(const uint8_t* bt_addr, uint8_t red, uint8_t green, uint8_t blue);



private:
    void poll();
    void pumpDirectOutput();

    void reconsiderBackendRefusal();
    uint32_t m_refusal_recheck = 0;
    void pumpDirectState(uint32_t now);
    bool writeOutputReport(const uint8_t* address, const uint8_t* data, uint16_t length);
    bool writeDirectFrame(uint8_t* frame, uint16_t length);
    bool directWriteFailed();

    static constexpr int kMaxCouchOutputs = 3;

    struct CouchOutputTarget {
        bool     in_use     = false;
        uint8_t  bt_addr[6] = {};
        uint16_t vendor_id  = 0;
        uint16_t product_id = 0;

        bool     owns_output  = false;
        uint32_t owns_recheck = 0;

        uint16_t rumble        = 0;
        bool     have_triggers = false;
        uint8_t  trigger_left[11]  = {};
        uint8_t  trigger_right[11] = {};
        bool     have_lightbar = false;
        uint8_t  lightbar[3]   = {};

        uint16_t sent          = 0;
        bool     sent_valid    = false;
        uint32_t sent_ms       = 0;
        uint8_t  trigger_last[22] = {};
        bool     trigger_sent  = false;
        uint8_t  intensity_last = 0;

        bool     lightbar_painted_valid = false;
        uint8_t  lightbar_painted[3]    = {};
        uint32_t lightbar_painted_ms    = 0;

        uint8_t  seq      = 0;
        uint32_t failures = 0;
    };

    CouchOutputTarget* findCouch(const uint8_t* bt_addr);
    void pumpCouchOutputs(uint32_t now);
    void ensureCouchOwnership(CouchOutputTarget& t, bool want, bool regime_on);
    bool writeCouchFrame(CouchOutputTarget& t, uint8_t* frame, uint16_t length);
    void paintCouchLightbar(CouchOutputTarget& t, uint32_t now);

    CouchOutputTarget m_couch[kMaxCouchOutputs]{};
    Mutex             m_couch_lock{};
    void logStatus(const char* when);
    void logRawReport(const char* when);

    static constexpr uint32_t kStatusLogInterval = 600;

    static constexpr uint64_t kValidBit = 1ull << 16;

    static uint64_t pack(uint8_t l2, uint8_t r2, bool valid, uint32_t ms);
    static uint32_t nowMs();

    static constexpr uint32_t kMaxAgeMs = 200;

    static constexpr uint64_t kPollIntervalNs = 8'333'333ull;

    static constexpr uint32_t kDirectWriteIntervalMs = 33;

    static constexpr uint32_t kDirectFailureLimit = 16;

    enum class DirectDiscoveryState {
        Unknown,
        Failed,
        Empty,
        Candidates,
    };

    mutable Mutex         m_srv_lock{};

    std::atomic<uint32_t> m_direct_seq{0};

    std::atomic<bool>     m_direct_wanted{false};
    std::atomic<uint16_t> m_direct_rumble{0};

    std::atomic<uint32_t> m_direct_trigger_seq{0};
    uint8_t               m_direct_trigger_left[11]{};
    uint8_t               m_direct_trigger_right[11]{};

    uint8_t  m_direct_addr[6]{};
    bool     m_direct_addr_valid = false;
    DirectDiscoveryState m_direct_discovery_state = DirectDiscoveryState::Unknown;
    bool     m_owns_output       = false;

    static std::atomic<bool> s_output_refused;

    uint32_t m_owns_recheck = 0;
    uint16_t m_direct_sent       = 0;
    bool     m_direct_sent_valid = false;
    uint32_t m_direct_sent_ms    = 0;
    uint32_t m_direct_failures   = 0;
    bool     m_direct_logged     = false;
    std::atomic<uint8_t> m_console_vibration{
        (uint8_t)akira::input::Ds5EffectIntensity::Strong};
    std::atomic<uint8_t> m_console_trigger{
        (uint8_t)akira::input::Ds5EffectIntensity::Strong};

    static constexpr uint32_t kGameLightbarSet = 0x01000000u;
    std::atomic<uint32_t> m_game_lightbar{0};

    uint32_t m_lightbar_painted       = 0;
    bool     m_lightbar_painted_valid = false;
    uint32_t m_lightbar_painted_ms    = 0;

    static constexpr uint32_t kLightbarIntervalMs = 250;

    uint8_t  m_direct_intensity_last = akira::input::kDs5IntensityFull;
    uint8_t  m_direct_trigger_last[22]{};
    bool     m_direct_trigger_sent    = false;
    uint32_t m_direct_trigger_sent_ms = 0;
    bool     m_direct_trigger_logged  = false;

    std::atomic<bool> m_haptics_landing{false};
    uint64_t m_haptics_written_seen = 0;
    uint64_t m_haptics_failed_seen  = 0;

    uint64_t m_reports_seen = 0;

    static constexpr int kMaxRawSlots = libchiaking::controllers::SeatNumber::count;

    struct RawSlot {
        mutable std::atomic<uint32_t> seq{0};
        AkiraInputRawReport           report{};
        std::atomic<uint32_t>         published_ms{0};
    };

    RawSlot m_raw_slots[kMaxRawSlots];

    uint8_t m_raw_slot_addr[kMaxRawSlots][6]{};
    bool    m_raw_slot_used[kMaxRawSlots]{};

    uint8_t m_tracked_addr[kMaxRawSlots][6]{};
    uint8_t m_tracked_count = 0;
    uint32_t m_tracked_refresh = 0;

    bool copyRawSlot(const RawSlot& slot, AkiraInputRawReport* out) const;
    int  claimRawSlot(const uint8_t* bt_addr);
    void publishRawReport(const AkiraInputRawReport& raw);
    void refreshTrackedDevices();

    std::atomic<bool>             m_raw_wanted{false};

    std::atomic<uint64_t> m_snapshot{0};
    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_degraded_notice{false};

    Service      m_srv{};
    libchiaking::platform::Thread m_thread;
    bool         m_subscribed = false;
    Availability m_availability = Availability::NotInstalled;
    uint32_t     m_mc_version = 0;
};

#endif // AKIRA_INPUT_EXTENDED_INPUT_MANAGER_HPP
