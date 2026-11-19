#pragma once

#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <libchiaking/play.hpp>

#include "platform/pads/pad.hpp"
#include "play/rumble_profile.hpp"

namespace akira::play {
class InputManager;
}
namespace akira::play {
struct StreamStats;
}

namespace akira::platform
{

class PadProvider
{
public:
    using Callbacks = std::vector<std::function<void()>>;

    virtual ~PadProvider() = default;

    std::mutex& mutex() const { return m_mutex; }

    virtual void open() = 0;
    virtual void close() = 0;
    virtual platform::Pad* acquire(Callbacks& callbacks) = 0;
    virtual platform::Pad* current() = 0;
    virtual std::vector<platform::PadDescription> scanPads() const = 0;
    virtual play::RumbleProfile primaryRumbleProfile() const = 0;

    virtual void readSecondaryPads(play::InputManager& input, std::span<libchiaking::controllers::ControllerInput> states,
                                   uint8_t& count) = 0;
    virtual void routeSeatFeedback(uint8_t slot, const libchiaking::controllers::ControllerFeedback& feedback) = 0;
    virtual void fillStreamStats(play::StreamStats& stats) = 0;

private:
    mutable std::mutex m_mutex;
};

}
