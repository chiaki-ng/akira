#include "core/background_work.hpp"

#include <borealis.hpp>

#include <utility>
#include <vector>

namespace akira {

BackgroundWork::Pause::Pause()
{
    hold();
}

BackgroundWork::Pause::~Pause()
{
    release();
}

BackgroundWork::Subscription::Subscription(Subscription&& other) noexcept
    : id(std::exchange(other.id, 0))
{
}

auto BackgroundWork::Subscription::operator=(Subscription&& other) noexcept -> Subscription&
{
    if (this != &other)
    {
        if (id != 0)
            unsubscribe(id);
        id = std::exchange(other.id, 0);
    }
    return *this;
}

BackgroundWork::Subscription::~Subscription()
{
    if (id != 0)
        unsubscribe(id);
}

auto BackgroundWork::paused() -> bool
{
    std::lock_guard lock(mutex);
    return holds > 0;
}

auto BackgroundWork::subscribe(Listener listener) -> Subscription
{
    std::lock_guard lock(mutex);
    const uint64_t id = nextId++;
    listeners.emplace(id, std::move(listener));
    return Subscription{id};
}

void BackgroundWork::hold()
{
    bool first = false;
    {
        std::lock_guard lock(mutex);
        first = holds++ == 0;
    }
    if (first)
    {
        brls::Logger::info("Background work: paused for the stream");
        announce(true);
    }
}

void BackgroundWork::release()
{
    bool last = false;
    {
        std::lock_guard lock(mutex);
        last = holds > 0 && --holds == 0;
    }
    if (last)
    {
        brls::Logger::info("Background work: resumed after the stream");
        announce(false);
    }
}

void BackgroundWork::announce(bool paused)
{
    std::vector<Listener> current;
    {
        std::lock_guard lock(mutex);
        for (const auto& [id, listener] : listeners)
            current.push_back(listener);
    }
    for (const auto& listener : current)
        listener(paused);
}

void BackgroundWork::unsubscribe(uint64_t id)
{
    std::lock_guard lock(mutex);
    listeners.erase(id);
}

}
