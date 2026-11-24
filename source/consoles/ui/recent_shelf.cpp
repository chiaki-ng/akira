#include "consoles/ui/recent_shelf.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "consoles/console_entry.hpp"

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
    std::vector<Banner> kept = catalogName == state.get().catalogName ? state.get().gallery : std::vector<Banner>{};
    state.set(View{ .catalogName = std::move(catalogName), .console = std::move(console), .titles = played.titles(),
        .launching = state.get().launching, .gallery = std::move(kept) });
}

void RecentShelf::exhibit(Banner banner)
{
    const View& shown = state.get();
    if (banner.catalogName != shown.catalogName || std::ranges::contains(shown.gallery, banner.titleId, &Banner::titleId))
        return;
    std::vector<Banner> gallery = shown.gallery;
    gallery.push_back(std::move(banner));
    const auto rank = [&titles = shown.titles](const Banner& listed) {
        return std::ranges::distance(titles.begin(), std::ranges::find(titles, listed.titleId, &RecentPlay::Title::titleId));
    };
    std::ranges::sort(gallery, {}, rank);
    state.write(&View::gallery, std::move(gallery));
}

auto RecentShelf::loadingArt(const std::string& console) const -> std::vector<Banner>
{
    const View& shown = state.get();
    if (shown.launching)
    {
        const auto chosen = std::ranges::find(shown.gallery, *shown.launching, &Banner::titleId);
        if (chosen != shown.gallery.end())
            return { *chosen };
        if (const std::optional<Banner> banner = launchArt())
            return { *banner };
        return {};
    }
    if (shown.console.empty() || (console != shown.console && console != shown.console + std::string(ConsoleEntry::remoteSuffix)))
        return {};
    return shown.gallery;
}

void RecentShelf::showBanner(Banner banner)
{
    if (banner.catalogName != state.get().catalogName)
        return;
    state.write(&View::banner, std::optional<Banner>{ std::move(banner) });
}

void RecentShelf::launch(std::string titleId)
{
    state.write(&View::launching, std::optional<std::string>{ std::move(titleId) });
}

void RecentShelf::landed()
{
    if (state.get().launching)
        state.write(&View::launching, std::optional<std::string>{});
}

auto RecentShelf::launchArt() const -> std::optional<Banner>
{
    const View& shown = state.get();
    if (!shown.launching || !shown.banner || shown.banner->titleId != *shown.launching)
        return std::nullopt;
    return shown.banner;
}

void RecentShelf::clear()
{
    if (!state.get().titles.empty() || !state.get().catalogName.empty())
        state.set(View{});
}

}
