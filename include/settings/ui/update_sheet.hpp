#pragma once

#include <optional>
#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "settings/maintenance.hpp"
#include "settings/ui/settings_model.hpp"

namespace akira::settings::ui {

class UpdateSheet final : public ::arisa::composition::Component {
public:
    UpdateSheet(Maintenance::UpdateCheck update, std::optional<Maintenance::UpdateProgress> progress, SettingsModel& model,
        const app::Strings& strings);

    bool operator==(const UpdateSheet& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    static constexpr float notesHeight = 280.0F;

    auto offer() const -> ::arisa::composition::Element;
    auto installing(const Maintenance::UpdateProgress& shown) const -> ::arisa::composition::Element;
    auto stage(const Maintenance::UpdateProgress& shown) const -> ::arisa::composition::Element;
    auto notes() const -> ::arisa::composition::Element;
    auto transferred(const Maintenance::UpdateProgress& shown) const -> std::string;
    static auto settled(const Maintenance::UpdateProgress& shown) -> bool;

    Maintenance::UpdateCheck update;
    std::optional<Maintenance::UpdateProgress> progress;
    SettingsModel* model;
    const app::Strings* strings;
};

}
