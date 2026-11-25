#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "app/ui_dispatcher.hpp"

namespace tests {

class FakeUiDispatcher : public akira::app::UiDispatcher {
public:
    void post(std::function<void()> work) override { work(); }

    void postAfter(std::chrono::milliseconds, std::function<void()> work) override { work(); }

    void runOnWorker(std::function<void()> work) override { work(); }

    auto repeatEvery(std::chrono::milliseconds, std::function<void()> work) -> std::unique_ptr<Repeating> override
    {
        auto repeat = std::make_unique<Repeat>(std::move(work));
        repeats.push_back(repeat->work);
        return repeat;
    }

    void tickRepeats()
    {
        std::erase_if(repeats, [](const std::weak_ptr<std::function<void()>>& repeat) { return repeat.expired(); });
        for (const auto& repeat : repeats)
            if (const auto work = repeat.lock())
                (*work)();
    }

private:
    class Repeat final : public Repeating {
    public:
        explicit Repeat(std::function<void()> work)
            : work(std::make_shared<std::function<void()>>(std::move(work)))
        {
        }

        std::shared_ptr<std::function<void()>> work;
    };

    std::vector<std::weak_ptr<std::function<void()>>> repeats;
};

}
