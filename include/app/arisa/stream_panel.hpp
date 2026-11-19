#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"

namespace akira::app::arisa {

class StreamDriver;

class StreamPanel final : public ::arisa::composition::Component {
public:
    StreamPanel(StreamDriver& driver, const app::Strings& strings, bool sleepOffered);

    bool operator==(const StreamPanel& other) const;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    StreamDriver* driver;
    const app::Strings* strings;
    bool sleepOffered;
};

}
