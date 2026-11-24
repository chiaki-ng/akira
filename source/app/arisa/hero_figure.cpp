#include "app/arisa/hero_figure.hpp"

#include <arisa/components/async_image.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>

#include <algorithm>
#include <numeric>
#include <span>

#include "app/arisa/art_session.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;

HeroFigure::HeroFigure(std::string url, Placement placement)
    : url(std::move(url))
    , placement(placement)
{
}

auto HeroFigure::body(composition::Context&) const -> composition::Element
{
    return components::GeometryReader([url = url, placement = placement](const composition::GeometryProxy& proxy) {
        const composition::Size area = proxy.size();
        return composition::Element(components::AsyncImage(ArtSession::request(url), components::AsyncImageOptions{},
                                        [area, placement](const components::AsyncImagePhase& phase) { return placed(phase, area, placement); }))
            .frame({ .width = area.width, .height = area.height });
    });
}

auto HeroFigure::placed(const components::AsyncImagePhase& phase, composition::Size area, Placement placement) -> composition::Element
{
    const std::shared_ptr<const ::arisa::provider::Image> pixels = phase.pixels();
    if (!pixels || pixels->width() == 0 || pixels->height() == 0)
        return components::Group();
    const Bounds figure = bounds(pixels);
    const float height = area.height * placement.figureHeight / std::max(figure.height, thinnest);
    const float width = height * static_cast<float>(pixels->width()) / static_cast<float>(pixels->height());
    const float centre = area.width * placement.centre - std::max(0.0F, area.width - placement.roomyWidth) * placement.spareShift;
    const float left = centre - width * (figure.x + figure.width / 2.0F);
    const float top = area.height * placement.figureMiddle - height * (figure.y + figure.height / 2.0F);
    return composition::Element(components::Image(pixels).resizable())
        .frame({ .width = width, .height = height })
        .position(left + width / 2.0F, top + height / 2.0F);
}

auto HeroFigure::bounds(const std::shared_ptr<const ::arisa::provider::Image>& image) -> Bounds
{
    using Entry = std::pair<std::weak_ptr<const ::arisa::provider::Image>, Bounds>;
    static std::vector<Entry> kept;
    std::erase_if(kept, [](const Entry& entry) { return entry.first.expired(); });
    const auto found = std::ranges::find_if(kept, [&image](const Entry& entry) { return entry.first.lock() == image; });
    if (found != kept.end())
        return found->second;
    const Bounds made = measured(*image);
    if (kept.size() >= boundsKept)
        kept.erase(kept.begin());
    kept.emplace_back(image, made);
    return made;
}

auto HeroFigure::measured(const ::arisa::provider::Image& image) -> Bounds
{
    const std::uint32_t width = image.width();
    const std::uint32_t height = image.height();
    const std::uint32_t step = std::max<std::uint32_t>(1, std::max(width, height) / sample);
    const std::uint32_t columns = (width + step - 1) / step;
    const std::uint32_t rows = (height + step - 1) / step;
    const std::span<const std::byte> rgba = image.pixels();
    std::vector<std::uint64_t> across(columns, 0);
    std::vector<std::uint64_t> down(rows, 0);
    for (std::uint32_t row = 0; row < rows; ++row)
        for (std::uint32_t column = 0; column < columns; ++column)
        {
            const std::size_t at = (static_cast<std::size_t>(row * step) * width + column * step) * ::arisa::provider::Image::channels + 3;
            const auto alpha = std::to_integer<std::uint64_t>(rgba[at]);
            across[column] += alpha;
            down[row] += alpha;
        }
    const auto horizontal = bulk(across);
    const auto vertical = bulk(down);
    if (!horizontal || !vertical)
        return Bounds{};
    return Bounds{ .x = static_cast<float>(horizontal->first) / static_cast<float>(columns),
        .y = static_cast<float>(vertical->first) / static_cast<float>(rows),
        .width = static_cast<float>(horizontal->second - horizontal->first) / static_cast<float>(columns),
        .height = static_cast<float>(vertical->second - vertical->first) / static_cast<float>(rows) };
}

auto HeroFigure::bulk(const std::vector<std::uint64_t>& weights) -> std::optional<std::pair<std::size_t, std::size_t>>
{
    const std::uint64_t total = std::accumulate(weights.begin(), weights.end(), std::uint64_t{ 0 });
    if (total == 0)
        return std::nullopt;
    const std::uint64_t trimmed = total * trimPercent / 100;
    std::size_t first = 0;
    std::size_t last = weights.size() - 1;
    std::uint64_t before = 0;
    while (first < last && before + weights[first] <= trimmed)
        before += weights[first++];
    std::uint64_t after = 0;
    while (last > first && after + weights[last] <= trimmed)
        after += weights[last--];
    return std::pair{ first, last + 1 };
}

}
