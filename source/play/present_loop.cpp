#include "play/present_loop.hpp"

#include "log/log.hpp"

namespace akira::play {

PresentLoop::PresentLoop(Target& target, Pacing pacing)
    : target(target)
    , pacing(pacing)
    , repainter([this](std::stop_token stop) { repaint(stop); })
{
}

PresentLoop::~PresentLoop()
{
    stop();
}

void PresentLoop::stop()
{
    repainter.request_stop();
    wake.notify_all();
    if (repainter.joinable())
        repainter.join();
}

void PresentLoop::frameReady()
{
    std::unique_lock lock(mutex);
    present(PresentTiming::Kind::frame, lock);
}

void PresentLoop::paused(bool paused)
{
    held.store(paused, std::memory_order_release);
    nudges.fetch_add(1, std::memory_order_acq_rel);
    wake.notify_all();
}

void PresentLoop::serialized(std::function<void()> work)
{
    if (presenting.load(std::memory_order_acquire) == std::this_thread::get_id())
    {
        work();
        return;
    }
    const std::scoped_lock lock(mutex);
    work();
}

auto PresentLoop::totals() const -> PresentTiming::Totals
{
    const std::scoped_lock lock(mutex);
    return timing.totals();
}

void PresentLoop::present(PresentTiming::Kind kind, std::unique_lock<std::mutex>&)
{
    const Clock::time_point ready = Clock::now();
    const std::uint32_t wanted = held.load(std::memory_order_acquire) ? pacing.pausedInterval : pacing.liveInterval;
    if (appliedInterval != wanted)
    {
        target.swapInterval(wanted);
        timing.swapInterval(wanted);
        appliedInterval = wanted;
    }
    presenting.store(std::this_thread::get_id(), std::memory_order_release);
    const std::expected<Presented, PresentTiming::Miss> shown = target.present(kind, timing);
    presenting.store(std::thread::id{}, std::memory_order_release);
    if (!shown)
    {
        timing.missed(shown.error());
        return;
    }
    timing.presented(kind, PresentTiming::Stamps{ .ready = ready, .submitted = shown->submitted, .presented = shown->presented });
    lastPresent = shown->presented;
    if (const std::optional<std::string> line = timing.report(shown->presented))
        log::info("{}", *line);
}

void PresentLoop::repaint(std::stop_token stop)
{
    std::unique_lock lock(mutex);
    while (!stop.stop_requested())
    {
        if (held.load(std::memory_order_acquire) && lastPresent && followScenes(stop, lock))
            continue;
        const std::uint64_t seen = nudges.load(std::memory_order_acquire);
        static_cast<void>(wake.wait_for(lock, stop, pacing.stallRepaint,
            [this, seen] { return nudges.load(std::memory_order_acquire) != seen; }));
        if (stop.stop_requested())
            return;
        if (!lastPresent || Clock::now() - *lastPresent < pacing.stallRepaint)
            continue;
        present(PresentTiming::Kind::repaint, lock);
    }
}

auto PresentLoop::followScenes(std::stop_token stop, std::unique_lock<std::mutex>& lock) -> bool
{
    lock.unlock();
    const Scene scene = target.awaitScene(pacing.sceneWait);
    lock.lock();
    if (scene == Scene::Absent)
        return false;
    if (scene == Scene::Fresh && !stop.stop_requested() && held.load(std::memory_order_acquire))
        present(PresentTiming::Kind::repaint, lock);
    return true;
}

}
