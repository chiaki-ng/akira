#include "app/arisa/window.hpp"

#include <utility>
#include <vector>

namespace akira::app::arisa {

Window::Window(::arisa::provider::SurfaceProvider& surface, ::arisa::provider::LifecycleProvider& lifecycle)
    : surface(surface)
    , lifecycle(lifecycle)
    , phase(lifecycle.phase())
{
}

auto Window::width() const -> unsigned
{
    return surface.size().width.value();
}

auto Window::height() const -> unsigned
{
    return surface.size().height.value();
}

void Window::suspendRendering()
{
    videoStarted.store(true, std::memory_order_release);
}

auto Window::takeVideoStart() -> bool
{
    return videoStarted.exchange(false, std::memory_order_acq_rel);
}

auto Window::watchFocus(std::function<void(bool focused)> changed) -> std::unique_ptr<FocusWatch>
{
    return std::make_unique<Watch>(watchers, watchers->add(std::move(changed)));
}

void Window::tick()
{
    const ::arisa::provider::ScenePhase now = lifecycle.phase();
    if (now == phase)
        return;
    const bool wasActive = phase == ::arisa::provider::ScenePhase::active;
    phase = now;
    const bool active = now == ::arisa::provider::ScenePhase::active;
    if (active != wasActive)
        watchers->notify(active);
}

auto Window::Watchers::add(std::function<void(bool focused)> changed) -> std::uint64_t
{
    std::lock_guard lock(mutex);
    const std::uint64_t id = next++;
    watchers.emplace(id, std::make_shared<std::function<void(bool focused)>>(std::move(changed)));
    return id;
}

void Window::Watchers::remove(std::uint64_t id)
{
    std::lock_guard lock(mutex);
    watchers.erase(id);
}

void Window::Watchers::notify(bool focused)
{
    std::vector<std::shared_ptr<std::function<void(bool focused)>>> current;
    {
        std::lock_guard lock(mutex);
        for (const auto& [id, changed] : watchers)
            current.push_back(changed);
    }
    for (const auto& changed : current)
        (*changed)(focused);
}

Window::Watch::Watch(std::shared_ptr<Watchers> watchers, std::uint64_t id)
    : watchers(std::move(watchers))
    , id(id)
{
}

Window::Watch::~Watch()
{
    watchers->remove(id);
}

}
