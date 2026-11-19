#include "cloud/library_status.hpp"

namespace akira::cloud {

LibraryStatus::LibraryStatus(const Status& status)
    : availability(status.availability)
    , pairable(status.canPair)
{
}

auto LibraryStatus::chip() const -> std::optional<Chip>
{
    switch (availability)
    {
        case Availability::Ready: return Chip::Ready;
        case Availability::CatalogAvailable: return Chip::Catalog;
        case Availability::SubscriptionRequired: return Chip::Unavailable;
        case Availability::Warning:
        case Availability::LaunchBlocked: return Chip::Warning;
        case Availability::Empty: return Chip::Empty;
        case Availability::NeedsPairing: return Chip::Pair;
        case Availability::Checking: return Chip::Checking;
        case Availability::Error: return Chip::Error;
        case Availability::NoProfile: break;
    }
    return std::nullopt;
}

auto LibraryStatus::glyph() const -> Glyph
{
    switch (availability)
    {
        case Availability::NeedsPairing: return Glyph::Pair;
        case Availability::Error:
        case Availability::LaunchBlocked: return Glyph::Alert;
        default: return Glyph::Cloud;
    }
}

auto LibraryStatus::tone() const -> Tone
{
    switch (availability)
    {
        case Availability::NeedsPairing:
        case Availability::Checking: return Tone::Accent;
        case Availability::Error:
        case Availability::LaunchBlocked: return Tone::Danger;
        default: return Tone::Dim;
    }
}

auto LibraryStatus::action() const -> Action
{
    switch (availability)
    {
        case Availability::NeedsPairing: return Action::Pair;
        case Availability::Empty:
        case Availability::Error:
        case Availability::LaunchBlocked: return Action::Refresh;
        default: return Action::None;
    }
}

auto LibraryStatus::options() const -> std::vector<Option>
{
    std::vector<Option> shown{Option::Refresh};
    if (pairable)
        shown.push_back(Option::Pair);
    shown.push_back(Option::ClearCache);
    return shown;
}

}
