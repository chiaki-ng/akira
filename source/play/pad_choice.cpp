#include "play/pad_choice.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

namespace akira::play {

PadChoice::PadChoice(std::vector<CouchTable::Pad> pads, Clock::time_point now)
    : listed(std::move(pads))
    , released(listed.size(), false)
    , opened(now)
{
}

auto PadChoice::differ(const CouchTable::Pad& left, const CouchTable::Pad& right) -> bool
{
    return left.kind != right.kind || left.triggers != right.triggers || left.touchpad != right.touchpad;
}

auto PadChoice::worthAsking(const std::vector<CouchTable::Pad>& pads) -> bool
{
    if (pads.size() < 2)
        return false;
    return std::ranges::any_of(pads | std::views::drop(1),
        [&first = pads.front()](const CouchTable::Pad& pad) { return differ(first, pad); });
}

auto PadChoice::fallback(const std::vector<CouchTable::Pad>& pads) -> std::optional<PadId>
{
    const auto found = std::ranges::find_if(pads, [](const CouchTable::Pad& pad) { return pad.triggers || pad.touchpad; });
    if (found == pads.end())
        return std::nullopt;
    return found->id;
}

auto PadChoice::capabilities(const CouchTable::Pad& pad) -> std::vector<Capability>
{
    std::vector<Capability> shown;
    if (pad.touchpad)
        shown.push_back(Capability::Touchpad);
    if (pad.triggers)
        shown.push_back(Capability::Triggers);
    if (pad.motion)
        shown.push_back(Capability::Motion);
    if (shown.empty())
        shown.push_back(Capability::Digital);
    return shown;
}

auto PadChoice::sameSet(const std::vector<CouchTable::Pad>& left, const std::vector<CouchTable::Pad>& right) -> bool
{
    return std::ranges::equal(left, right,
        [](const CouchTable::Pad& a, const CouchTable::Pad& b) { return a.id == b.id && a.kind == b.kind; });
}

auto PadChoice::choice() const -> std::optional<PadId>
{
    if (!chosen || *chosen >= listed.size())
        return std::nullopt;
    return listed[*chosen].id;
}

auto PadChoice::carriedOver(const std::optional<CouchTable::Pad>& before) const -> std::optional<std::size_t>
{
    if (!before)
        return std::nullopt;
    const auto at = [this](auto found) -> std::optional<std::size_t> {
        if (found == listed.end())
            return std::nullopt;
        return static_cast<std::size_t>(found - listed.begin());
    };
    if (auto same = at(std::ranges::find_if(listed,
            [&before](const CouchTable::Pad& pad) { return pad.id == before->id && pad.kind == before->kind; })))
        return same;
    if (before->vendor != 0)
        if (auto model = at(std::ranges::find_if(listed, [&before](const CouchTable::Pad& pad) {
                return pad.vendor == before->vendor && pad.product == before->product;
            })))
            return model;
    if (before->kind == CouchTable::PadKind::JoyCon
        && std::ranges::count(listed, CouchTable::PadKind::JoyCon, &CouchTable::Pad::kind) == 1)
        return at(std::ranges::find(listed, CouchTable::PadKind::JoyCon, &CouchTable::Pad::kind));
    return std::nullopt;
}

auto PadChoice::refresh(std::vector<CouchTable::Pad> fresh) -> bool
{
    if (sameSet(fresh, listed))
        return false;
    std::optional<CouchTable::Pad> before;
    if (chosen && *chosen < listed.size())
        before = listed[*chosen];
    listed = std::move(fresh);
    released.assign(listed.size(), false);
    chosen = carriedOver(before);
    return true;
}

auto PadChoice::sample(const std::function<bool(PadId)>& picking) -> bool
{
    for (std::size_t index = 0; index < listed.size(); ++index)
    {
        if (!picking(listed[index].id))
        {
            released[index] = true;
            continue;
        }
        if (!released[index])
            continue;
        if (chosen == index)
            return false;
        chosen = index;
        return true;
    }
    return false;
}

auto PadChoice::idle(Clock::time_point now) const -> bool
{
    return now - opened >= idleLimit;
}

}
