#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "input/pad.hpp"

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

private:
    mutable std::mutex m_mutex;
};

}
