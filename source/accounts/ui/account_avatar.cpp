#include "accounts/ui/account_avatar.hpp"

#include <arisa/components/async_image.hpp>
#include <arisa/components/async_image_phase.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <cctype>
#include <optional>
#include <utility>

#include "app/arisa/art_session.hpp"

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

AccountAvatar::AccountAvatar(std::string name, std::string url, float side)
    : name(std::move(name))
    , url(std::move(url))
    , side(side)
{
}

auto AccountAvatar::initial(const std::string& name) -> std::string
{
    if (name.empty())
        return "?";
    return std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(name.front()))));
}

auto AccountAvatar::monogram(const std::string& name, float side) -> composition::Element
{
    return components::Text(initial(name))
        .font(theme::Font::system(side * 0.42F, theme::Font::Weight::semibold))
        .foreground_style(theme::ColourRole::on_accent)
        .frame({ .width = side, .height = side })
        .background(theme::ColourRole::accent, theme::Shape::circle());
}

auto AccountAvatar::body(composition::Context&) const -> composition::Element
{
    std::optional<components::UrlRequest> request = app::arisa::ArtSession::request(url);
    if (!request)
        return monogram(name, side);
    return composition::Element(components::AsyncImage(std::move(request), components::AsyncImageOptions{},
                                    [name = name, side = side](const components::AsyncImagePhase& phase) -> composition::Element {
                                        if (std::optional<components::Image> image = phase.image())
                                            return std::move(*image).resizable().scaled_to_fill();
                                        return monogram(name, side);
                                    }))
        .frame({ .width = side, .height = side })
        .clip_shape(theme::Shape::circle());
}

}
