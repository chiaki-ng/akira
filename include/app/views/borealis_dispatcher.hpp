#pragma once

#include <borealis/core/timer.hpp>

#include "app/ui_dispatcher.hpp"

namespace akira::app::views {

class BorealisDispatcher : public UiDispatcher {
public:
    void post(std::function<void()> work) override;
    void postAfter(std::chrono::milliseconds delay, std::function<void()> work) override;
    void runOnWorker(std::function<void()> work) override;
    auto repeatEvery(std::chrono::milliseconds interval, std::function<void()> work)
        -> std::unique_ptr<Repeating> override;

private:
    class Timer final : public Repeating {
    public:
        Timer(std::chrono::milliseconds interval, std::function<void()> work);

    private:
        brls::RepeatingTimer timer;
    };
};

}
