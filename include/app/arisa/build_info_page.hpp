#pragma once

#include <filesystem>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/build_info.hpp"
#include "app/strings.hpp"

namespace akira::app::arisa {

class BuildInfoPage final : public ::arisa::composition::Component {
public:
    BuildInfoPage(std::filesystem::path file, const app::Strings& strings);

    bool operator==(const BuildInfoPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static auto line(const std::string& text) -> ::arisa::composition::Element;
    static auto section(const BuildInfo::Section& shown) -> ::arisa::composition::Element;

    std::filesystem::path file;
    const app::Strings* strings;
};

}
