#include "app/arisa/overlay_handoff.hpp"

#include <utility>

namespace akira::app::arisa {

void OverlayHandoff::offer(::arisa::provider::FrameSource frames, ::arisa::provider::SurfaceSize extent)
{
    const std::scoped_lock lock(mutex);
    pending.emplace(Offer{ .frames = std::move(frames), .extent = extent });
}

void OverlayHandoff::refuse(std::string reason)
{
    const std::scoped_lock lock(mutex);
    pending.emplace(Refusal{ .reason = std::move(reason) });
}

void OverlayHandoff::withdraw()
{
    const std::scoped_lock lock(mutex);
    if (pending && std::holds_alternative<Offer>(*pending))
        pending.reset();
}

auto OverlayHandoff::take() -> std::optional<Event>
{
    const std::scoped_lock lock(mutex);
    return std::exchange(pending, std::nullopt);
}

}
