#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <libchiaking/trophies_types.hpp>

namespace akira::trophies {

class TrophyListing {
public:
    using Trophy = libchiaking::trophies::Trophy;
    using Group = libchiaking::trophies::TrophyGroup;
    using GroupId = libchiaking::trophies::TrophyGroupId;
    using Detail = libchiaking::trophies::TitleTrophies;
    using Grade = libchiaking::trophies::Grade;
    using Rarity = libchiaking::trophies::Rarity;

    enum class Sort { Progress, DateObtained, Grade, Rarity, Name };
    enum class Filter { All, Earned, Unearned, Hidden };

    static constexpr std::array<Sort, 5> sorts{ Sort::Progress, Sort::DateObtained, Sort::Grade, Sort::Rarity,
        Sort::Name };
    static constexpr std::array<Filter, 4> filters{ Filter::All, Filter::Earned, Filter::Unearned, Filter::Hidden };
    static constexpr std::array<Grade, 4> grades{ Grade::platinum, Grade::gold, Grade::silver, Grade::bronze };

    TrophyListing() = default;
    TrophyListing(Sort sort, Filter filter, std::optional<GroupId> group);

    auto sort() const -> Sort { return order; }
    auto filter() const -> Filter { return shown; }
    auto group() const -> const std::optional<GroupId>& { return chosen; }
    auto listed(const Detail& detail) const -> std::vector<Trophy>;

    static auto grouped(const Detail& detail) -> bool { return detail.groups.size() > 1; }
    static auto groupOf(const Detail& detail, const GroupId& id) -> std::optional<Group>;
    static auto matches(const Trophy& trophy, Filter filter) -> bool;
    static auto concealed(const Trophy& trophy) -> bool { return trophy.hidden && !trophy.earned; }
    static auto progressPercent(const Trophy& trophy) -> std::uint32_t;
    static auto gradeRank(Grade grade) -> int;
    static auto sortKey(Sort sort) -> std::string_view;
    static auto filterKey(Filter filter) -> std::string_view;
    static auto gradeKey(Grade grade) -> std::string_view;
    static auto gradeAsset(Grade grade) -> std::string_view;
    static auto rarityKey(Rarity rarity) -> std::string_view;

    friend bool operator==(const TrophyListing&, const TrophyListing&) = default;

private:
    static void sortBy(std::vector<Trophy>& trophies, Sort sort);

    Sort order = Sort::Progress;
    Filter shown = Filter::All;
    std::optional<GroupId> chosen;
};

}
