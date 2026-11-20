#include "app/arisa/artwork.hpp"

#include <arisa/components/async_image.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>

#include "app/arisa/art_session.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

Artwork::Artwork(std::string url, std::string symbol)
    : url(std::move(url))
    , symbol(std::move(symbol))
{
}

auto Artwork::body(composition::Context&) const -> composition::Element
{
    return components::AsyncImage(ArtSession::request(url), components::AsyncImageOptions{},
        [symbol = symbol](const components::AsyncImagePhase& phase) { return shown(phase, symbol); })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity });
}

auto Artwork::shown(const components::AsyncImagePhase& phase, const std::string& symbol) -> composition::Element
{
    if (std::optional<components::Image> image = phase.image())
        return std::move(*image).resizable().scaled_to_fill();
    return blank(symbol);
}

auto Artwork::blank(const std::string& symbol) -> composition::Element
{
    return components::ZStack(components::Image(components::Image::SystemName{ symbol })
                                  .font(theme::Font::title)
                                  .foreground_style(theme::ColourRole::tertiary_label))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourRole::secondary_system_fill);
}

}
