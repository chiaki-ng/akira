#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "accounts/ui/account_model.hpp"
#include "app/strings.hpp"
#include "settings/help_catalog.hpp"

namespace akira::accounts::ui {

class AccountSection final : public ::arisa::composition::Component {
public:
    static constexpr float avatarSize = 56.0F;
    static constexpr float rowAvatarSize = 30.0F;

    AccountSection(AccountModel& model, const settings::HelpCatalog& help, const app::Strings& strings);

    bool operator==(const AccountSection& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto initial(const std::string& name) -> std::string;
    static auto kindKey(AccountDesk::Kind kind) -> const char*;

private:
    auto card(const AccountModel::View& view) const -> ::arisa::composition::Element;
    auto profiles(const AccountModel::View& view) const -> ::arisa::composition::Element;
    auto identity(const AccountModel::View& view) const -> ::arisa::composition::Element;
    auto companion(const AccountModel::View& view) const -> ::arisa::composition::Element;
    auto tokens(const AccountModel::View& view) const -> ::arisa::composition::Element;
    auto confirmations(::arisa::composition::Element screen, const AccountModel::View& view) const
        -> ::arisa::composition::Element;
    auto credential(const AccountDesk::Credential& shown) const -> ::arisa::composition::Element;
    auto refreshLabel(const AccountDesk::Refresh& refresh) const -> std::string;
    auto described(::arisa::composition::Element row, std::string_view id) const -> ::arisa::composition::Element;
    static auto avatar(const std::string& name, float side) -> ::arisa::composition::Element;
    static auto section(std::string header, std::vector<::arisa::composition::Element> rows) -> ::arisa::composition::Element;

    AccountModel* model;
    const settings::HelpCatalog* help;
    const app::Strings* strings;
};

}
