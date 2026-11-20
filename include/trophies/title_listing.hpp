#pragma once

#include <array>
#include <string_view>
#include <vector>

#include <libchiaking/trophies_types.hpp>

namespace akira::trophies {

class TitleListing {
public:
    using Title = libchiaking::trophies::TrophyTitle;
    using Platform = libchiaking::trophies::TrophyPlatform;

    enum class Sort { Recent, Progress, Name, Earned };
    enum class Filter { All, InProgress, Completed, Unstarted, Hidden };

    static constexpr std::array<Sort, 4> sorts{ Sort::Recent, Sort::Progress, Sort::Name, Sort::Earned };
    static constexpr std::array<Filter, 5> filters{ Filter::All, Filter::InProgress, Filter::Completed,
        Filter::Unstarted, Filter::Hidden };

    TitleListing() = default;
    TitleListing(Sort sort, Filter filter);

    auto sort() const -> Sort { return order; }
    auto filter() const -> Filter { return shown; }
    auto listed(const std::vector<Title>& titles) const -> std::vector<Title>;

    static auto matches(const Title& title, Filter filter) -> bool;
    static auto sortKey(Sort sort) -> std::string_view;
    static auto filterKey(Filter filter) -> std::string_view;
    static auto platformName(Platform platform) -> std::string_view;

    friend bool operator==(const TitleListing&, const TitleListing&) = default;

private:
    static auto updatedLater(const Title& left, const Title& right) -> bool;

    Sort order = Sort::Recent;
    Filter shown = Filter::All;
};

}
