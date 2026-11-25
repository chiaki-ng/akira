#include "app/arisa/artwork.hpp"

#include <arisa/components/async_image.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "app/arisa/art_session.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

Artwork::Artwork(std::string url, std::string symbol, Scaling scaling)
    : url(std::move(url))
    , symbol(std::move(symbol))
    , scaling(scaling)
{
}

auto Artwork::body(composition::Context&) const -> composition::Element
{
    return components::AsyncImage(ArtSession::request(url), components::AsyncImageOptions{},
        [symbol = symbol, scaling = scaling](const components::AsyncImagePhase& phase) { return shown(phase, symbol, scaling); })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity });
}

auto Artwork::shown(const components::AsyncImagePhase& phase, const std::string& symbol, Scaling scaling)
    -> composition::Element
{
    if (scaling == Scaling::Trimmed)
    {
        if (const std::shared_ptr<const ::arisa::provider::Image> pixels = phase.pixels())
            return components::Image(trimmed(pixels)).resizable().scaled_to_fit();
        return blank(symbol);
    }
    if (std::optional<components::Image> image = phase.image())
        return scaling == Scaling::Fill ? std::move(*image).resizable().scaled_to_fill()
                                        : std::move(*image).resizable().scaled_to_fit();
    return blank(symbol);
}

auto Artwork::trimmed(const std::shared_ptr<const ::arisa::provider::Image>& source)
    -> std::shared_ptr<const ::arisa::provider::Image>
{
    using Entry = std::pair<std::weak_ptr<const ::arisa::provider::Image>, std::shared_ptr<const ::arisa::provider::Image>>;
    static std::vector<Entry> kept;
    std::erase_if(kept, [](const Entry& entry) { return entry.first.expired(); });
    const auto found = std::ranges::find_if(kept, [&source](const Entry& entry) { return entry.first.lock() == source; });
    if (found != kept.end())
        return found->second;
    std::shared_ptr<const ::arisa::provider::Image> made = trim(*source);
    if (!made)
        made = source;
    if (kept.size() >= trimmedKept)
        kept.erase(kept.begin());
    kept.emplace_back(source, made);
    return made;
}

auto Artwork::trim(const ::arisa::provider::Image& source) -> std::shared_ptr<const ::arisa::provider::Image>
{
    const std::uint32_t width = source.width();
    const std::uint32_t height = source.height();
    const std::span<const std::byte> rgba = source.pixels();
    std::uint32_t left = width;
    std::uint32_t right = 0;
    std::uint32_t top = height;
    std::uint32_t bottom = 0;
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto alpha = std::to_integer<std::uint8_t>(rgba[(static_cast<std::size_t>(y) * width + x) * ::arisa::provider::Image::channels + 3]);
            if (alpha < visibleAlpha)
                continue;
            left = std::min(left, x);
            right = std::max(right, x + 1);
            top = std::min(top, y);
            bottom = std::max(bottom, y + 1);
        }
    if (left >= right || top >= bottom || (left == 0 && top == 0 && right == width && bottom == height))
        return nullptr;
    const std::uint32_t cropWidth = right - left;
    const std::size_t rowBytes = static_cast<std::size_t>(cropWidth) * ::arisa::provider::Image::channels;
    std::vector<std::byte> cropped;
    cropped.reserve(rowBytes * (bottom - top));
    for (std::uint32_t y = top; y < bottom; ++y)
    {
        const auto row = rgba.subspan((static_cast<std::size_t>(y) * width + left) * ::arisa::provider::Image::channels, rowBytes);
        cropped.insert(cropped.end(), row.begin(), row.end());
    }
    auto made = ::arisa::provider::Image::create(cropWidth, bottom - top, std::move(cropped));
    if (!made)
        return nullptr;
    return std::make_shared<const ::arisa::provider::Image>(std::move(*made));
}

auto Artwork::blank(const std::string& symbol) -> composition::Element
{
    if (symbol.empty())
        return components::Group();
    return components::ZStack(components::Image(components::Image::SystemName{ symbol })
                                  .font(theme::Font::title)
                                  .foreground_style(theme::ColourRole::tertiary_label))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourRole::secondary_system_fill);
}

}
