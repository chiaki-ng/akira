#pragma once

#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "network/network_facts.hpp"

namespace akira::app::arisa {

class Header final : public ::arisa::composition::Component {
public:
    static constexpr float spacing = 14.0F;
#if AKIRA_PLATFORM_OUNCE
    static constexpr const char* platformTag = "OUNCE";
#else
    static constexpr const char* platformTag = "NX";
#endif

    Header(std::string title, const app::Strings& strings, std::optional<network::NetworkFacts> facts = std::nullopt);

    bool operator==(const Header& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    std::string title;
    std::optional<network::NetworkFacts> facts;
    const app::Strings* strings;
};

}
