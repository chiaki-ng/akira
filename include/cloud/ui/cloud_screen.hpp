#pragma once

#include <optional>

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "cloud/ui/cloud_model.hpp"

namespace akira::cloud::ui {

class CloudScreen final : public ::arisa::composition::Component {
public:
    static constexpr float codeSide = 220.0F;
    static constexpr float codeWidth = 520.0F;

    CloudScreen(CloudModel& model, const app::Strings& strings);

    bool operator==(const CloudScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

private:
    auto coded() const -> ::arisa::composition::Binding<bool>;
    auto choosing() const -> ::arisa::composition::Binding<bool>;
    static auto codeSheet(const std::optional<CloudModel::Code>& code, CloudModel* model, const app::Strings* strings)
        -> ::arisa::composition::Element;
    static auto bundleChoices(const std::optional<CloudModel::Detail>& detail, CloudModel* model,
        const app::Strings* strings) -> ::arisa::composition::Element;

    CloudModel* model;
    const app::Strings* strings;
};

}
