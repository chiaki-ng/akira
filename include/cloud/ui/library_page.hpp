#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/colour_value.hpp>

#include "app/strings.hpp"
#include "cloud/library_status.hpp"
#include "cloud/ui/cloud_model.hpp"

namespace akira::cloud::ui {

class LibraryPage final : public ::arisa::composition::Component {
public:
    static constexpr std::size_t columns = 5;
    static constexpr float spacing = 18.0F;
    static constexpr float coverRadius = 12.0F;
    static constexpr float coverSide = 180.0F;
    static constexpr float searchWidth = 240.0F;
    static constexpr float stateWidth = 560.0F;
    static constexpr float rowExtent = 270.0F;

    LibraryPage(CloudModel& model, const app::Strings& strings);

    bool operator==(const LibraryPage& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto toolbar(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto filters(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto grid(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto statePanel(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto loadingLines(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto statusChip(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto sortPicker(const CloudModel::View& view) const -> ::arisa::composition::Element;
    auto optionsMenu(const CloudModel::View& view) const -> ::arisa::composition::Element;
    static auto card(const Game& game, CloudModel* model, const app::Strings* strings) -> ::arisa::composition::Element;
    static auto statusLine(const Game& game, CloudModel* model, const app::Strings* strings) -> ::arisa::composition::Element;
    static auto chipColour(LibraryStatus::Chip chip) -> ::arisa::theme::ColourValue;
    static auto platformTitle(PlatformFilter platform, const app::Strings* strings) -> std::string;

    CloudModel* model;
    const app::Strings* strings;
};

}
