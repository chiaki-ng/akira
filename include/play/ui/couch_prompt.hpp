#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "play/ui/couch.hpp"

namespace akira::play::ui {

class CouchPrompt final : public ::arisa::composition::Component {
public:
    static constexpr float width = 520.0F;

    CouchPrompt(Couch& couch, const app::Strings& strings);

    bool operator==(const CouchPrompt& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    Couch* couch;
    const app::Strings* strings;
};

}
