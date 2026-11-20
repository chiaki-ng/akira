#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "log/log_excerpt.hpp"
#include "log/ui/log_shelf.hpp"

namespace akira::log::ui {

class LogBrowser final : public ::arisa::composition::Component {
public:
    LogBrowser(LogShelf& shelf, const app::Strings& strings);

    bool operator==(const LogBrowser& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto tone(LogExcerpt::Level level) -> ::arisa::theme::ColourValue;

private:
    auto folder(const LogShelf::View& view) const -> ::arisa::composition::Element;
    auto file(const LogShelf::View& view) const -> ::arisa::composition::Element;
    auto row(const LogFolder::Entry& entry) const -> ::arisa::composition::Element;
    auto status(const LogShelf::View& view) const -> std::string;

    LogShelf* shelf;
    const app::Strings* strings;
};

}
