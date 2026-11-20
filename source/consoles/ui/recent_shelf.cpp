#include "consoles/ui/recent_shelf.hpp"

#include <algorithm>
#include <utility>

namespace akira::consoles::ui {

auto RecentShelf::title(const std::string& titleId) const -> std::optional<RecentPlay::Title>
{
    const std::vector<RecentPlay::Title>& titles = state.get().titles;
    const auto found = std::ranges::find(titles, titleId, &RecentPlay::Title::titleId);
    if (found == titles.end())
        return std::nullopt;
    return *found;
}

void RecentShelf::show(std::string catalogName, std::string console, const RecentPlay& played)
{
    state.set(View{ .catalogName = std::move(catalogName), .console = std::move(console), .titles = played.titles() });
}

void RecentShelf::clear()
{
    if (!state.get().titles.empty() || !state.get().catalogName.empty())
        state.set(View{});
}

}
