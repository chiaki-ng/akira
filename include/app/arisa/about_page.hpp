#pragma once

#include <array>
#include <string>
#include <string_view>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"

namespace akira::app::arisa {

class AboutPage final : public ::arisa::composition::Component {
public:
    struct Link {
        std::string_view name;
        std::string_view url;
        std::string_view author;
        std::string_view noteKey;
    };

    static constexpr float codeSide = 260.0F;
    static constexpr Link maintainer{ "xlanor", "https://github.com/xlanor", "", "" };
    static constexpr std::array<Link, 2> community{ {
        { "Discord", "https://discord.gg/tAMbRuwXDH", "", "akira/about/discord_note" },
        { "GitHub", "https://github.com/chiaki-ng/akira", "", "akira/about/github_note" },
    } };
    static constexpr std::array<Link, 6> thanks{ {
        { "chiaki-ng", "https://github.com/streetpea/chiaki-ng", "Street Pea", "" },
        { "chiaki", "https://git.sr.ht/~thestr4ng3r/chiaki", "thestr4ng3r", "akira/about/chiaki_note" },
        { "Pylux", "https://github.com/ForWard-Technologies-LLC/Pylux", "Forward Technologies LLC", "" },
        { "Pxplay", "https://streamingdv.github.io/pxplay/", "grill2010", "" },
        { "Cloudpad", "https://github.com/Chazq2023/CloudPad-Android", "Chaz", "" },
        { "borealis", "https://github.com/natinusala/borealis", "natinusala and contributors", "akira/about/borealis_note" },
    } };
    static const ::arisa::composition::StateKey<std::string> shown;

    explicit AboutPage(const app::Strings& strings);

    bool operator==(const AboutPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto figures() const -> ::arisa::composition::Element;
    auto link(const Link& target, ::arisa::composition::StateHandle<std::string> open) const -> ::arisa::composition::Element;
    static auto code(const std::string& url, ::arisa::composition::StateHandle<std::string> open, const app::Strings& strings)
        -> ::arisa::composition::Element;
    static auto figure(std::string label, std::string value) -> ::arisa::composition::Element;

    const app::Strings* strings;
};

}
