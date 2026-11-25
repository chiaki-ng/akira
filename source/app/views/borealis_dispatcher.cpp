#include "app/views/borealis_dispatcher.hpp"

#include <borealis/core/thread.hpp>

#include <utility>

namespace akira::app::views {

void BorealisDispatcher::post(std::function<void()> work)
{
    brls::sync(work);
}

void BorealisDispatcher::postAfter(std::chrono::milliseconds delay, std::function<void()> work)
{
    brls::delay(static_cast<long>(delay.count()), work);
}

void BorealisDispatcher::runOnWorker(std::function<void()> work)
{
    brls::async(work);
}

auto BorealisDispatcher::repeatEvery(std::chrono::milliseconds interval, std::function<void()> work)
    -> std::unique_ptr<Repeating>
{
    return std::make_unique<Timer>(interval, std::move(work));
}

BorealisDispatcher::Timer::Timer(std::chrono::milliseconds interval, std::function<void()> work)
{
    timer.setCallback(std::move(work));
    timer.start(static_cast<brls::Time>(interval.count()));
}

}
