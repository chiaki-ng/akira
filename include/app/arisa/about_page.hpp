#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/assets/provider.hpp>

#include "app/strings.hpp"

namespace akira::app::arisa {

class AboutPage final : public ::arisa::composition::Component {
public:
    struct Project {
        std::string_view name;
        std::string_view url;
        std::string_view author;
        std::string_view logo;
        std::string_view symbol;
        std::string_view noteKey;
    };

    struct Tile {
        std::string_view titleKey;
        std::string_view detailKey;
        std::string_view captionKey;
        std::string_view url;
        std::string_view mark;
        std::string_view symbol;
        std::uint32_t tint;
    };

    static constexpr float codeSide = 260.0F;
    static constexpr float logoSide = 64.0F;
    static constexpr float markSide = 44.0F;
    static constexpr float tileRadius = 16.0F;
    static constexpr std::size_t projectColumns = 4;
    static constexpr std::size_t contributorColumns = 5;
    static constexpr std::string_view contributorsFile = "about/chiaki-ng-CONTRIBUTORS.txt";
    static constexpr std::array<std::string_view, 2> leadingContributors{ "streetpea", "thestr4ng3r" };
    static constexpr std::array<Tile, 2> tiles{ {
        { "akira/about/discord_title", "akira/about/discord_detail", "akira/about/discord_preferred",
            "https://discord.gg/tAMbRuwXDH", "img/brand/discord-white.png", "forum", 0x5865F2 },
        { "akira/about/github_title", "akira/about/github_detail", "", "https://github.com/chiaki-ng/akira",
            "img/brand/github-white.png", "code", 0x24292E },
    } };
    static constexpr std::array<Project, 4> projects{ {
        { "chiaki-ng", "https://github.com/streetpea/chiaki-ng", "Street Pea", "img/chiaking.png", "", "" },
        { "Chiaki", "https://git.sr.ht/~thestr4ng3r/chiaki", "thestr4ng3r", "img/brand/chiaki.png", "", "" },
        { "Pylux", "https://github.com/ForWard-Technologies-LLC/Pylux", "Forward Technologies LLC",
            "https://raw.githubusercontent.com/ForWard-Technologies-LLC/Pylux/master/pylux-logo.png", "", "" },
        { "Pxplay", "https://streamingdv.github.io/pxplay/", "grill2010",
            "https://cdn2.steamgriddb.com/icon_thumb/5f2e8fb6fcccffa6b083f5b411e11221.png", "", "" },
    } };
    static const ::arisa::composition::StateKey<std::string> shown;
    static const ::arisa::composition::StateKey<bool> contributorsShown;

    AboutPage(const app::Strings& strings, const std::vector<std::string>& contributors);

    bool operator==(const AboutPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto contributors(const ::arisa::provider::AssetProvider& assets) -> std::vector<std::string>;
    static auto ordered(std::vector<std::string> logins) -> std::vector<std::string>;

private:
    auto figures() const -> ::arisa::composition::Element;
    auto tile(const Tile& shown, ::arisa::composition::StateHandle<std::string> open) const -> ::arisa::composition::Element;
    auto project(const Project& shown, ::arisa::composition::StateHandle<std::string> open) const
        -> ::arisa::composition::Element;
    auto people(::arisa::composition::StateHandle<std::string> open, ::arisa::composition::StateHandle<bool> expanded) const
        -> ::arisa::composition::Element;
    auto thanks(::arisa::composition::StateHandle<std::string> open, ::arisa::composition::StateHandle<bool> expanded) const
        -> ::arisa::composition::Element;
    static auto rows(std::vector<::arisa::composition::Element> cells, std::size_t columns, float spacing)
        -> ::arisa::composition::Element;
    static auto mark(const Tile& shown) -> ::arisa::composition::Element;
    static auto code(const std::string& url, ::arisa::composition::StateHandle<std::string> open, const app::Strings& strings)
        -> ::arisa::composition::Element;
    static auto figure(std::string label, std::string value) -> ::arisa::composition::Element;

    const app::Strings* strings;
    const std::vector<std::string>* logins;
};

}
