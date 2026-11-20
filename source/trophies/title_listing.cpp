#include "trophies/title_listing.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>

namespace akira::trophies {

TitleListing::TitleListing(Sort sort, Filter filter)
    : order(sort)
    , shown(filter)
{
}

auto TitleListing::matches(const Title& title, Filter filter) -> bool
{
    const std::uint64_t earned = title.earned.total();
    const std::uint64_t defined = title.defined.total();
    switch (filter)
    {
        case Filter::All: return !title.hidden;
        case Filter::InProgress: return !title.hidden && earned > 0 && earned < defined;
        case Filter::Completed: return !title.hidden && defined > 0 && earned >= defined;
        case Filter::Unstarted: return !title.hidden && earned == 0;
        case Filter::Hidden: return title.hidden;
    }
    return true;
}

auto TitleListing::updatedLater(const Title& left, const Title& right) -> bool
{
    return left.last_updated > right.last_updated;
}

auto TitleListing::listed(const std::vector<Title>& titles) const -> std::vector<Title>
{
    std::vector<Title> rows = titles
        | std::views::filter([this](const Title& title) { return matches(title, shown); })
        | std::ranges::to<std::vector>();
    switch (order)
    {
        case Sort::Recent:
            std::ranges::stable_sort(rows, updatedLater);
            break;
        case Sort::Progress:
            std::ranges::stable_sort(rows, [](const Title& left, const Title& right) {
                if (left.progress != right.progress)
                    return left.progress > right.progress;
                return updatedLater(left, right);
            });
            break;
        case Sort::Name:
            std::ranges::stable_sort(rows, [](const Title& left, const Title& right) { return left.name < right.name; });
            break;
        case Sort::Earned:
            std::ranges::stable_sort(rows, [](const Title& left, const Title& right) {
                const std::uint64_t leftEarned = left.earned.total();
                const std::uint64_t rightEarned = right.earned.total();
                if (leftEarned != rightEarned)
                    return leftEarned > rightEarned;
                return updatedLater(left, right);
            });
            break;
    }
    return rows;
}

auto TitleListing::sortKey(Sort sort) -> std::string_view
{
    switch (sort)
    {
        case Sort::Progress: return "akira/trophies/tsort_progress";
        case Sort::Name: return "akira/trophies/tsort_name";
        case Sort::Earned: return "akira/trophies/tsort_earned";
        case Sort::Recent: break;
    }
    return "akira/trophies/tsort_recent";
}

auto TitleListing::filterKey(Filter filter) -> std::string_view
{
    switch (filter)
    {
        case Filter::InProgress: return "akira/trophies/tfilter_in_progress";
        case Filter::Completed: return "akira/trophies/tfilter_completed";
        case Filter::Unstarted: return "akira/trophies/tfilter_unstarted";
        case Filter::Hidden: return "akira/trophies/tfilter_hidden";
        case Filter::All: break;
    }
    return "akira/trophies/tfilter_all";
}

auto TitleListing::platformName(Platform platform) -> std::string_view
{
    switch (platform)
    {
        case Platform::ps3: return "PS3";
        case Platform::ps4: return "PS4";
        case Platform::ps5: return "PS5";
        case Platform::ps_vita: return "PS Vita";
        case Platform::pc: return "PC";
    }
    return "";
}

}
