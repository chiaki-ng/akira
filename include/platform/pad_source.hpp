#pragma once

#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <libchiaking/play.hpp>

#include "input/pad.hpp"

class InputManager;
struct StreamStats;

namespace akira::platform
{

class PadSource
{
public:
    using Callbacks = std::vector<std::function<void()>>;

    virtual ~PadSource() = default;

    std::mutex& mutex() const { return m_mutex; }

    virtual void open() = 0;
    virtual void close() = 0;
    virtual input::Pad* acquire(Callbacks& callbacks) = 0;
    virtual input::Pad* current() = 0;
    virtual std::vector<input::PadDescription> scanPads() const = 0;

    virtual bool seatsAllowed() const = 0;
    virtual void readSecondaryPads(InputManager& input, std::span<libchiaking::controllers::ControllerInput> states,
                                   uint8_t& count) = 0;
    virtual void routeSeatFeedback(uint8_t slot, const libchiaking::controllers::ControllerFeedback& feedback) = 0;
    virtual void fillStreamStats(StreamStats& stats) = 0;

private:
    mutable std::mutex m_mutex;
};

}
