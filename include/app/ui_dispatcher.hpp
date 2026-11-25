#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace akira::app {

class UiDispatcher {
public:
    class Repeating {
    public:
        virtual ~Repeating() = default;
    };

    virtual ~UiDispatcher() = default;

    virtual void post(std::function<void()> work) = 0;
    virtual void postAfter(std::chrono::milliseconds delay, std::function<void()> work) = 0;
    virtual void runOnWorker(std::function<void()> work) = 0;
    virtual auto repeatEvery(std::chrono::milliseconds interval, std::function<void()> work)
        -> std::unique_ptr<Repeating> = 0;
};

}
