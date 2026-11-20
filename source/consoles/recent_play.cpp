#include "consoles/recent_play.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace akira::consoles {

RecentPlay::RecentPlay(std::vector<Title> titles)
{
    std::vector<Title> played;
    std::ranges::copy_if(titles, std::back_inserter(played), [](const Title& title) { return title.recentlyPlayed; });
    if (played.empty())
        played = std::move(titles);
    std::ranges::stable_sort(played, [](const Title& left, const Title& right) {
        if (left.lastUsed.has_value() != right.lastUsed.has_value())
            return left.lastUsed.has_value();
        return left.lastUsed && *left.lastUsed > *right.lastUsed;
    });
    if (played.size() > limit)
        played.resize(limit);
    chosen = std::move(played);
}

auto RecentPlay::find(const std::string& titleId) const -> std::optional<Title>
{
    const auto found = std::ranges::find(chosen, titleId, &Title::titleId);
    if (found == chosen.end())
        return std::nullopt;
    return *found;
}

auto RecentPlay::age(Clock::time_point used, Clock::time_point now) -> Age
{
    using std::chrono::days;
    const auto today = std::chrono::floor<days>(now);
    const auto day = std::chrono::floor<days>(used);
    const int elapsed = static_cast<int>(std::max<days::rep>(0, (today - day).count()));
    if (elapsed == 0)
        return Age{ .span = Span::Today };
    if (elapsed == 1)
        return Age{ .span = Span::Yesterday, .count = 1 };
    if (elapsed < 7)
        return Age{ .span = Span::Days, .count = elapsed };
    if (elapsed < 30)
        return Age{ .span = Span::Weeks, .count = elapsed / 7 };
    if (elapsed < 365)
        return Age{ .span = Span::Months, .count = elapsed / 30 };
    return Age{ .span = Span::Years, .count = elapsed / 365 };
}

auto RecentPlay::ageKey(Span span) -> const char*
{
    switch (span)
    {
        case Span::Today: return "akira/hosts/recent_today";
        case Span::Yesterday: return "akira/hosts/recent_yesterday";
        case Span::Days: return "akira/hosts/recent_days";
        case Span::Weeks: return "akira/hosts/recent_weeks";
        case Span::Months: return "akira/hosts/recent_months";
        case Span::Years: break;
    }
    return "akira/hosts/recent_years";
}

}
