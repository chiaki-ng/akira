#pragma once

#include <functional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"

namespace akira::accounts::ui {

class Welcome final : public ::arisa::composition::Component {
public:
    static constexpr float tileSide = 132.0F;
    static constexpr float tileRadius = 18.0F;
    static constexpr float tileSpacing = 16.0F;

    Welcome(AccountModel& model, const app::Strings& strings);

    bool operator==(const Welcome& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    struct Tile {
        std::string key;
        std::string symbol;
        std::string label;
        std::string helpTitle;
        std::string helpBody;
        bool prominent = false;
        std::function<void()> chosen;
    };

    static auto tile(Tile shown) -> ::arisa::composition::Element;

    AccountModel* model;
    const app::Strings* strings;
};

}
