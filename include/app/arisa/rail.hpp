#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/arisa/shell_model.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class Rail final : public ::arisa::composition::Component {
public:
    static constexpr float inset = 16.0F;
    static constexpr float collapsedWidth = 64.0F;
    static constexpr float expandedWidth = 220.0F;
    static constexpr float padding = 10.0F;
    static constexpr float stopHeight = 44.0F;
    static constexpr float stopSpacing = 6.0F;
    static constexpr float stopRadius = 13.0F;
    static constexpr float iconSize = 17.0F;
    static constexpr float iconColumn = 28.0F;
    static constexpr float labelSpacing = 14.0F;
    static constexpr float stopInset = 10.0F;
    static constexpr float selectedOpacity = 0.22F;
    static constexpr std::size_t menuStop = 0;
    static constexpr ::arisa::composition::FocusState<std::optional<std::size_t>> stopFocus{ "rail.focus" };
    static constexpr ::arisa::composition::StateKey<bool> pinned{ "rail.pinned", false };

    Rail(ShellModel& model, const app::Strings& strings);

    bool operator==(const Rail& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    struct Stop {
        std::string title;
        std::string symbol;
        bool selected = false;
        std::move_only_function<void()> perform;
    };

    auto stop(Stop shown, std::size_t index, bool expanded,
        ::arisa::composition::FocusBinding<std::optional<std::size_t>> focus) const -> ::arisa::composition::Element;
    static auto pages(const ShellModel::View& view) -> std::vector<ShellModel::Page>;
    auto title(ShellModel::Page page) const -> std::string;
    static auto symbol(ShellModel::Page page) -> std::string;

    ShellModel* model;
    const app::Strings* strings;
};

}
