#pragma once

#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "network/network_facts.hpp"

namespace akira::network::ui {

class NetworkFactsRow final : public ::arisa::composition::Component {
public:
    static constexpr float factSpacing = 28.0F;
    static constexpr float dotSize = 8.0F;

    NetworkFactsRow(NetworkFacts facts, const app::Strings& strings);

    bool operator==(const NetworkFactsRow& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto fact(std::string label, ::arisa::composition::Element value) -> ::arisa::composition::Element;
    static auto joined(const std::vector<std::string>& servers) -> std::string;
    auto psn() const -> ::arisa::composition::Element;
    auto orMissing(std::string text) const -> std::string;

    NetworkFacts facts;
    const app::Strings* strings;
};

}
