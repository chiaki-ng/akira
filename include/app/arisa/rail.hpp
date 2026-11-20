#pragma once

#include <functional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class Rail final : public ::arisa::composition::Component {
public:
    static constexpr float width = 72.0F;
    static constexpr float inset = 16.0F;
    static constexpr float padding = 10.0F;
    static constexpr float spacing = 6.0F;
    static constexpr float iconSize = 21.0F;
    static constexpr float stopRadius = 13.0F;
    static constexpr float currentOpacity = 0.22F;

    Rail(ShellModel& model, const app::Strings& strings);

    bool operator==(const Rail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto stop(std::string title, std::string symbol, bool current, std::function<void()> chosen,
        std::string key) -> ::arisa::composition::Element;

    ShellModel* model;
    const app::Strings* strings;
};

}
