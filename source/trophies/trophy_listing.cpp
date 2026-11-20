#include "trophies/trophy_listing.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

namespace akira::trophies {

TrophyListing::TrophyListing(Sort sort, Filter filter, std::optional<GroupId> group)
    : order(sort)
    , shown(filter)
    , chosen(std::move(group))
{
}

auto TrophyListing::groupOf(const Detail& detail, const GroupId& id) -> std::optional<Group>
{
    const auto found = std::ranges::find(detail.groups, id, &Group::id);
    if (found == detail.groups.end())
        return std::nullopt;
    return *found;
}

auto TrophyListing::matches(const Trophy& trophy, Filter filter) -> bool
{
    switch (filter)
    {
        case Filter::All: return true;
        case Filter::Earned: return trophy.earned;
        case Filter::Unearned: return !trophy.earned;
        case Filter::Hidden: return trophy.hidden;
    }
    return true;
}

auto TrophyListing::progressPercent(const Trophy& trophy) -> std::uint32_t
{
    return trophy.progress ? trophy.progress->percent : 0;
}

auto TrophyListing::gradeRank(Grade grade) -> int
{
    switch (grade)
    {
        case Grade::platinum: return 0;
        case Grade::gold: return 1;
        case Grade::silver: return 2;
        case Grade::bronze: return 3;
    }
    return 4;
}

auto TrophyListing::listed(const Detail& detail) const -> std::vector<Trophy>
{
    const bool everyGroup = !grouped(detail) || !chosen;
    std::vector<Trophy> rows = detail.trophies
        | std::views::filter([this, everyGroup](const Trophy& trophy) {
              return (everyGroup || trophy.group == *chosen) && matches(trophy, shown);
          })
        | std::ranges::to<std::vector>();
    sortBy(rows, order);
    return rows;
}

void TrophyListing::sortBy(std::vector<Trophy>& trophies, Sort sort)
{
    switch (sort)
    {
        case Sort::Progress:
            std::ranges::stable_sort(trophies, [](const Trophy& left, const Trophy& right) {
                const bool leftMoving = !left.earned && progressPercent(left) > 0;
                const bool rightMoving = !right.earned && progressPercent(right) > 0;
                if (leftMoving != rightMoving)
                    return leftMoving;
                if (leftMoving && rightMoving)
                    return progressPercent(left) > progressPercent(right);
                if (left.earned != right.earned)
                    return !left.earned;
                return left.id < right.id;
            });
            return;
        case Sort::DateObtained:
            std::ranges::stable_sort(trophies, [](const Trophy& left, const Trophy& right) {
                const bool leftDated = left.earned && left.earned_at.has_value();
                const bool rightDated = right.earned && right.earned_at.has_value();
                if (leftDated != rightDated)
                    return leftDated;
                if (leftDated && rightDated)
                    return *left.earned_at > *right.earned_at;
                if (left.earned != right.earned)
                    return left.earned;
                return left.id < right.id;
            });
            return;
        case Sort::Grade:
            std::ranges::stable_sort(trophies, [](const Trophy& left, const Trophy& right) {
                const int leftRank = gradeRank(left.grade);
                const int rightRank = gradeRank(right.grade);
                if (leftRank != rightRank)
                    return leftRank < rightRank;
                return left.id < right.id;
            });
            return;
        case Sort::Rarity:
            std::ranges::stable_sort(trophies, [](const Trophy& left, const Trophy& right) {
                const double leftRate = left.earned_rate.value_or(0.0);
                const double rightRate = right.earned_rate.value_or(0.0);
                const bool leftRated = leftRate > 0.0;
                const bool rightRated = rightRate > 0.0;
                if (leftRated != rightRated)
                    return leftRated;
                if (leftRated && rightRated && leftRate != rightRate)
                    return leftRate < rightRate;
                if (left.rarity != right.rarity)
                    return left.rarity < right.rarity;
                return left.id < right.id;
            });
            return;
        case Sort::Name:
            std::ranges::stable_sort(trophies, [](const Trophy& left, const Trophy& right) {
                if (left.name != right.name)
                    return left.name < right.name;
                return left.id < right.id;
            });
            return;
    }
}

auto TrophyListing::sortKey(Sort sort) -> std::string_view
{
    switch (sort)
    {
        case Sort::DateObtained: return "akira/trophies/sort_date";
        case Sort::Grade: return "akira/trophies/sort_grade";
        case Sort::Rarity: return "akira/trophies/sort_rarity";
        case Sort::Name: return "akira/trophies/sort_name";
        case Sort::Progress: break;
    }
    return "akira/trophies/sort_progress";
}

auto TrophyListing::filterKey(Filter filter) -> std::string_view
{
    switch (filter)
    {
        case Filter::Earned: return "akira/trophies/filter_earned";
        case Filter::Unearned: return "akira/trophies/filter_unearned";
        case Filter::Hidden: return "akira/trophies/filter_hidden";
        case Filter::All: break;
    }
    return "akira/trophies/filter_all";
}

auto TrophyListing::gradeKey(Grade grade) -> std::string_view
{
    switch (grade)
    {
        case Grade::platinum: return "akira/trophies/type_platinum";
        case Grade::gold: return "akira/trophies/type_gold";
        case Grade::silver: return "akira/trophies/type_silver";
        case Grade::bronze: break;
    }
    return "akira/trophies/type_bronze";
}

auto TrophyListing::gradeAsset(Grade grade) -> std::string_view
{
    switch (grade)
    {
        case Grade::platinum: return "img/trophy/platinum.png";
        case Grade::gold: return "img/trophy/gold.png";
        case Grade::silver: return "img/trophy/silver.png";
        case Grade::bronze: break;
    }
    return "img/trophy/bronze.png";
}

auto TrophyListing::rarityKey(Rarity rarity) -> std::string_view
{
    switch (rarity)
    {
        case Rarity::ultra_rare: return "akira/trophies/rarity_ultra_rare";
        case Rarity::very_rare: return "akira/trophies/rarity_very_rare";
        case Rarity::rare: return "akira/trophies/rarity_rare";
        case Rarity::common: break;
    }
    return "akira/trophies/rarity_common";
}

}
