#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <vector>

#include <switch.h>

#include "input/controller_timing.hpp"
#include "input/sample_cadence.hpp"
#include "platform/nx/extended_input_manager.hpp"
#include "platform/nx/pad_path.hpp"
#include "platform/pad_source.hpp"

class InputManager;

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

namespace akira::platform
{

class NxPadSource final : public PadSource
{
public:
    static constexpr uint8_t kNoCouchSlot = 0xff;

    void open() override;
    void close() override;
    input::Pad* acquire(Callbacks& callbacks) override;
    input::Pad* current() override;
    std::vector<input::PadDescription> scanPads() const override;

    void readSecondaryPads(InputManager& input, std::span<libchiaking::controllers::ControllerInput> states, uint8_t& count);

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

    void setPath(std::unique_ptr<input::PadPath> path);
    PadPathInfo pathInfo() const;

    void sendTriggerEffects(const input::PadPath::TriggerEffect& left,
        const input::PadPath::TriggerEffect& right);
    void sendEffectIntensity(uint8_t vibration, uint8_t trigger);
    void sendLightbar(uint8_t red, uint8_t green, uint8_t blue);

    void selectNpad(HidNpadIdType npad);

    std::vector<input::PadDescription> describePads();

    ExtendedInputManager& extendedInput() { return m_extended; }

private:
    void setPathLocked(std::unique_ptr<input::PadPath> path);
    PadPathInfo pathInfoLocked() const;
    std::vector<input::PadDescription> describePadsLocked();
    uint8_t couchJoinLocked(HidNpadIdType npad);
    void couchLeaveLocked(HidNpadIdType npad);
    uint8_t couchPadCountLocked() const;
    bool isClaimableNpadLocked(HidNpadIdType npad) const;

    void retryPathIdentification();

    void reconcilePathDriver();

    void pollCouchClaim(Callbacks& callbacks);

    void pollPadArrivals(Callbacks& callbacks);

    uint64_t couchNpadMask() const;

    ExtendedInputManager m_extended;

    std::unique_ptr<input::PadPath> m_path;
    uint64_t m_path_generation = 0;

    static constexpr HidNpadIdType kNoNpad = (HidNpadIdType)0xff;
    HidNpadIdType m_bound_npad     = kNoNpad;
    std::map<HidNpadIdType, uint8_t> m_couch_roster;
    std::map<HidNpadIdType, std::unique_ptr<input::PadPath>> m_secondary_paths;
    std::map<HidNpadIdType, input::timing::TimePoint> m_secondary_missing_since;
    std::vector<input::PadDescription> m_couch_pads;
    input::timing::TimePoint m_couch_pads_read{};
    bool m_couch_claim_active = false;
    std::map<HidNpadIdType, input::timing::TimePoint> m_claim_started_at;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_joined;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_disconnected;
    std::function<void(HidNpadIdType)> m_on_pad_arrived;
    uint32_t m_connected_mask = 0;
    uint32_t m_announced_mask = 0;
    bool     m_arrival_primed = false;
    bool     m_arrival_pending = false;
    input::SampleCadence m_arrival_cadence{input::timing::ArrivalPollPeriod};
    bool          m_identify_done  = false;
    uint32_t      m_identify_next  = 0;
    uint32_t      m_identify_until = 0;
    uint32_t      m_driver_next    = 0;
};

}
