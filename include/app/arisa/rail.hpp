#pragma once

#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class Rail final : public ::arisa::composition::Component {
public:
    static constexpr float inset = 16.0F;

    Rail(ShellModel& model, const app::Strings& strings);

    bool operator==(const Rail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto pages(const ShellModel::View& view) -> std::vector<ShellModel::Page>;
    auto title(ShellModel::Page page) const -> std::string;
    static auto symbol(ShellModel::Page page) -> std::string;

    ShellModel* model;
    const app::Strings* strings;
};

}
