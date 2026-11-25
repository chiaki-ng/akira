#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

namespace akira::accounts::ui {

class AccountAvatar final : public ::arisa::composition::Component {
public:
    AccountAvatar(std::string name, std::string url, float side);

    bool operator==(const AccountAvatar& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto initial(const std::string& name) -> std::string;

private:
    static auto monogram(const std::string& name, float side) -> ::arisa::composition::Element;

    std::string name;
    std::string url;
    float side;
};

}
