#include "app/arisa/dispatcher.hpp"

#include <algorithm>
#include <utility>

namespace akira::app::arisa {

Dispatcher::Dispatcher(::arisa::runtime::Dispatcher root, ::arisa::async::FrameScheduler& timers,
    ::arisa::async::Executor& workers)
    : root(std::move(root))
    , timers(timers)
    , workers(workers)
{
}

Dispatcher::~Dispatcher()
{
    timed.clear();
    background.clear();
}

void Dispatcher::post(std::function<void()> work)
{
    static_cast<void>(root.post(std::move(work)));
}

void Dispatcher::postAfter(std::chrono::milliseconds delay, std::function<void()> work)
{
    post([this, delay, work = std::move(work)]() mutable {
        timed.keep(timers.after(delay, [work = std::move(work)](std::stop_token stop) {
            if (!stop.stop_requested())
                work();
        }));
    });
}

void Dispatcher::runOnWorker(std::function<void()> work)
{
    background.keep(workers.submit([work = std::move(work)](std::stop_token) { work(); }));
}

auto Dispatcher::repeatEvery(std::chrono::milliseconds interval, std::function<void()> work)
    -> std::unique_ptr<Repeating>
{
    return std::make_unique<Ticker>(*this, interval, std::move(work));
}

void Dispatcher::Jobs::keep(::arisa::async::Job job)
{
    std::lock_guard lock(mutex);
    std::erase_if(jobs, [](const ::arisa::async::Job& kept) { return kept.finished() || kept.dropped(); });
    jobs.push_back(std::move(job));
}

void Dispatcher::Jobs::clear()
{
    std::vector<::arisa::async::Job> released;
    {
        std::lock_guard lock(mutex);
        released.swap(jobs);
    }
}

Dispatcher::Ticker::Ticker(Dispatcher& dispatcher, std::chrono::milliseconds interval, std::function<void()> work)
    : state(std::make_shared<State>(interval, std::move(work)))
{
    schedule(dispatcher, state);
}

Dispatcher::Ticker::~Ticker()
{
    state->alive.store(false, std::memory_order_release);
}

void Dispatcher::Ticker::schedule(Dispatcher& dispatcher, std::shared_ptr<State> state)
{
    dispatcher.postAfter(state->interval, [&dispatcher, state] {
        if (!state->alive.load(std::memory_order_acquire))
            return;
        state->work();
        schedule(dispatcher, state);
    });
}

}
