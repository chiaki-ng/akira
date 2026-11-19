#include "play/views/stream_nav.hpp"
#include "log/log.hpp"

#include <borealis.hpp>

namespace akira::views::stream_nav {

namespace {
    constexpr size_t kNoBase = SIZE_MAX;
    size_t baseDepth = kNoBase;
}

void markBase()
{
    baseDepth = brls::Application::getActivitiesStack().size();
}

void clearBase()
{
    baseDepth = kNoBase;
}

bool active()
{
    return baseDepth != kNoBase;
}

void unwindToBase()
{
    if (baseDepth == kNoBase)
        return;

    const size_t depth = brls::Application::getActivitiesStack().size();
    if (depth <= baseDepth || depth <= 1)
    {
        akira::log::info("stream_nav: unwound to {}", depth);
        clearBase();
        return;
    }

    brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                   []() { unwindToBase(); });
}

} // namespace akira::views::stream_nav
