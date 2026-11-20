#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include "app/strings.hpp"
#include "play/ui/couch.hpp"

namespace akira::play::ui {

class SeatPasscode final : public ::arisa::composition::Component {
public:
    static constexpr std::size_t length = 4;
    static constexpr float width = 520.0F;
    static const ::arisa::composition::StateKey<std::string> entered;

    SeatPasscode(Couch& couch, const app::Strings& strings);

    bool operator==(const SeatPasscode& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto complete(const std::string& passcode) -> bool;

private:
    Couch* couch;
    const app::Strings* strings;
};

}
