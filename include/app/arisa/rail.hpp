#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/state_key.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/rail_edge.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class Rail final : public ::arisa::composition::Component {
public:
    static constexpr float inset = 16.0F;
    static constexpr float collapsedWidth = 64.0F;
    static constexpr float expandedWidth = 220.0F;
    static constexpr float barHeight = 64.0F;
    static constexpr float padding = 10.0F;
    static constexpr float stopHeight = 44.0F;
    static constexpr float stopSpacing = 6.0F;
    static constexpr float stopRadius = 13.0F;
    static constexpr float iconSize = 17.0F;
    static constexpr float iconColumn = 28.0F;
    static constexpr float avatarSide = 26.0F;
    static constexpr float labelSpacing = 14.0F;
    static constexpr float stopInset = 10.0F;
    static constexpr float selectedOpacity = 0.22F;
    static constexpr float dimOpacity = 0.32F;
    static constexpr ::arisa::composition::FocusState<std::size_t> stopFocus{ "rail.focus" };
    static constexpr ::arisa::composition::StateKey<bool> pinned{ "rail.pinned", false };

    Rail(ShellModel& model, const app::Strings& strings, accounts::ui::AccountModel* accounts, RailEdge edge);

    bool operator==(const Rail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto vertical(RailEdge edge) -> bool;

private:
    struct Look {
        bool expanded = false;
        bool vertical = true;
        ::arisa::composition::FocusBinding<std::size_t> focus;
    };

    auto stop(std::string title, std::string symbol, bool selected, std::function<void()> perform, std::size_t index,
        const Look& look) const -> ::arisa::composition::Element;
    auto profile(::arisa::composition::Context& context, std::size_t index, const Look& look) const
        -> std::optional<::arisa::composition::Element>;
    auto shaped(::arisa::composition::Element label, bool selected, const Look& look) const -> ::arisa::composition::Element;
    auto placed(::arisa::composition::Element panel, bool expanded) const -> ::arisa::composition::Element;
    static auto pages(const ShellModel::View& view) -> std::vector<ShellModel::Page>;
    auto title(ShellModel::Page page) const -> std::string;
    static auto symbol(ShellModel::Page page) -> std::string;

    ShellModel* model;
    const app::Strings* strings;
    accounts::ui::AccountModel* accounts;
    RailEdge edge;
};

}
