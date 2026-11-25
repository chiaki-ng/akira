#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include <switch.h>

#include "platform/pads/pad.hpp"

namespace akira::play
{

class CouchSeats
{
public:
    static constexpr uint8_t kNoCouchSlot = 0xff;

    virtual ~CouchSeats() = default;

    virtual std::vector<platform::PadDescription> describePads() = 0;
    virtual std::map<HidNpadIdType, uint8_t> couchRoster() const = 0;
    virtual uint8_t CouchPadIndexForController(HidNpadIdType npad) = 0;
    virtual bool couchSlotOutputTarget(uint8_t slot, uint8_t out_addr[6], uint16_t* vendor_id,
                                       uint16_t* product_id) = 0;
    virtual void unregisterCouchOutput(const uint8_t* bt_addr) = 0;

    virtual void couchLeave(HidNpadIdType npad) = 0;
    virtual void beginCouchJoin() = 0;
    virtual void cancelCouchJoin() = 0;
    virtual void tickCouchClaim() = 0;
    virtual float couchClaimProgress() const = 0;
    virtual bool isClaimableNpad(HidNpadIdType npad) const = 0;
    virtual HidNpadIdType boundNpad() const = 0;
    virtual void resolvePadArrival() = 0;
    virtual void selectNpad(HidNpadIdType npad) = 0;

    virtual void setOnCouchJoined(std::function<void(HidNpadIdType, uint8_t)> cb) = 0;
    virtual void setOnCouchDisconnected(std::function<void(HidNpadIdType, uint8_t)> cb) = 0;
    virtual void setOnPadArrived(std::function<void(HidNpadIdType)> cb) = 0;

    virtual bool backendAvailable() const = 0;
    virtual void logBackendDevices() = 0;
    virtual bool consumeDegradedNotice() = 0;
};

}
