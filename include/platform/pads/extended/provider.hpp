#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <vector>

#include <switch.h>

#include "platform/pads/extended/controller_timing.hpp"
#include "play/couch_seats.hpp"
#include "play/sample_cadence.hpp"
#include "platform/pads/extended/extended_input_manager.hpp"
#include "platform/pads/extended/pad_path.hpp"
#include "platform/pads/provider.hpp"

namespace akira::play {
class InputManager;
}

namespace akira::platform::extended
{

struct PadPathInfo {
    bool available = false;
    uint64_t generation = 0;
    akira::platform::PadPathKind kind = akira::platform::PadPathKind::JoyCon;
    HidNpadIdType npad = HidNpadIdType_No1;
    HidNpadStyleTag style = HidNpadStyleTag_NpadFullKey;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::array<uint8_t, 6> address{};
    bool hasAddress = false;
    bool nativeRumble = false;
    bool switchNative = true;
};

class ExtendedPadProvider final : public PadProvider, public play::CouchSeats
{
public:
    ExtendedPadProvider(const play::ControllerSettings& controllers,
                        libchiaking::platform::ThreadingProvider& threading);

    void open() override;
    void close() override;
    platform::Pad* acquire(Callbacks& callbacks) override;
    platform::Pad* current() override;
    std::vector<platform::PadDescription> scanPads() const override;
    play::RumbleProfile primaryRumbleProfile() const override;

    void readSecondaryPads(play::InputManager& input, std::span<libchiaking::controllers::ControllerInput> states,
                           uint8_t& count) override;
    void routeSeatFeedback(uint8_t slot, const libchiaking::controllers::ControllerFeedback& feedback) override;
    void fillStreamStats(play::StreamStats& stats) override;

    uint8_t couchJoin(HidNpadIdType npad);
    void couchLeave(HidNpadIdType npad) override;
    std::map<HidNpadIdType, uint8_t> couchRoster() const override;
    uint8_t couchPadCount() const;

    uint8_t CouchPadIndexForController(HidNpadIdType npad) override;

    bool couchSlotOutputTarget(uint8_t slot, uint8_t out_addr[6], uint16_t* vendor_id, uint16_t* product_id) override;
    void unregisterCouchOutput(const uint8_t* bt_addr) override;

    void beginCouchJoin() override;
    void cancelCouchJoin() override;
    void tickCouchClaim() override;
    bool couchClaimActive() const;
    float couchClaimProgress() const override;
    void setOnCouchJoined(std::function<void(HidNpadIdType, uint8_t)> cb) override;
    void setOnCouchDisconnected(std::function<void(HidNpadIdType, uint8_t)> cb) override;
    void setOnPadArrived(std::function<void(HidNpadIdType)> cb) override;
    void promptControllerSetup();
    bool isClaimableNpad(HidNpadIdType npad) const override;
    HidNpadIdType boundNpad() const override;
    void resolvePadArrival() override;

    void setPath(std::unique_ptr<PadPath> path);
    PadPathInfo pathInfo() const;

    void sendTriggerEffects(const PadPath::TriggerEffect& left,
        const PadPath::TriggerEffect& right);
    void sendEffectIntensity(uint8_t vibration, uint8_t trigger);
    void sendLightbar(uint8_t red, uint8_t green, uint8_t blue);

    void selectNpad(HidNpadIdType npad) override;

    std::vector<platform::PadDescription> describePads() override;

    bool backendAvailable() const override;
    void logBackendDevices() override;
    bool consumeDegradedNotice() override;

    ExtendedInputManager& extendedInput() { return m_extended; }

private:
    void setPathLocked(std::unique_ptr<PadPath> path);
    PadPathInfo pathInfoLocked() const;
    std::vector<platform::PadDescription> describePadsLocked();
    uint8_t couchJoinLocked(HidNpadIdType npad);
    void couchLeaveLocked(HidNpadIdType npad);
    uint8_t couchPadCountLocked() const;
    bool isClaimableNpadLocked(HidNpadIdType npad) const;

    void retryPathIdentification();

    void reconcilePathDriver();

    void pollCouchClaim(Callbacks& callbacks);

    void pollPadArrivals(Callbacks& callbacks);

    uint64_t couchNpadMask() const;

    const play::ControllerSettings& m_controllers;
    ExtendedInputManager m_extended;

    std::unique_ptr<PadPath> m_path;
    uint64_t m_path_generation = 0;

    static constexpr HidNpadIdType kNoNpad = (HidNpadIdType)0xff;
    HidNpadIdType m_bound_npad     = kNoNpad;
    std::map<HidNpadIdType, uint8_t> m_couch_roster;
    std::map<HidNpadIdType, std::unique_ptr<PadPath>> m_secondary_paths;
    std::map<HidNpadIdType, timing::TimePoint> m_secondary_missing_since;
    std::vector<platform::PadDescription> m_couch_pads;
    timing::TimePoint m_couch_pads_read{};
    bool m_couch_claim_active = false;
    std::map<HidNpadIdType, timing::TimePoint> m_claim_started_at;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_joined;
    std::function<void(HidNpadIdType, uint8_t)> m_on_couch_disconnected;
    std::function<void(HidNpadIdType)> m_on_pad_arrived;
    uint32_t m_connected_mask = 0;
    uint32_t m_announced_mask = 0;
    bool     m_arrival_primed = false;
    bool     m_arrival_pending = false;
    play::SampleCadence m_arrival_cadence{timing::ArrivalPollPeriod};
    bool          m_identify_done  = false;
    uint32_t      m_identify_next  = 0;
    uint32_t      m_identify_until = 0;
    uint32_t      m_driver_next    = 0;
};

}
