#include "cloud/ui/cloud_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

#include "app/arisa/qr_code.hpp"
#include "app/qr_matrix.hpp"
#include "cloud/ui/game_page.hpp"
#include "cloud/ui/library_page.hpp"

namespace akira::cloud::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

CloudScreen::CloudScreen(CloudModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto CloudScreen::body(composition::Context& context) const -> composition::Element
{
    const CloudModel::View& view = context.read(model->view());
    composition::Element page = view.detail ? composition::Element(GamePage(*model, *strings))
                                            : composition::Element(LibraryPage(*model, *strings));
    CloudModel* held = model;
    return components::VStack(components::VStackOptions{ .spacing = theme::space::none },
        std::move(page).frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .on_appear([held] { held->appear(); })
        .sheet(coded(),
            [code = view.code, held, shown = strings] {
                return codeSheet(code, held, shown).presentation_sizing(composition::PresentationSizing::fitted);
            })
        .confirmation_dialog(strings->text("akira/cloud/detail_choose_game"), choosing(),
            composition::Visibility::visible,
            [detail = view.detail, held, shown = strings] { return bundleChoices(detail, held, shown); })
        .id(composition::ElementKey{ "cloud.screen" });
}

auto CloudScreen::coded() const -> composition::Binding<bool>
{
    return composition::Binding<bool>([held = model] { return held->view().get().code.has_value(); },
        [held = model](bool shown) {
            if (!shown)
                held->dismissCode();
        });
}

auto CloudScreen::choosing() const -> composition::Binding<bool>
{
    return composition::Binding<bool>(
        [held = model] {
            const auto& detail = held->view().get().detail;
            return detail.has_value() && detail->choosing;
        },
        [held = model](bool shown) {
            if (!shown)
                held->dismissChoice();
        });
}

auto CloudScreen::codeSheet(const std::optional<CloudModel::Code>& code, CloudModel* model, const app::Strings* strings)
    -> composition::Element
{
    std::vector<composition::Element> rows;
    if (code)
    {
        rows.push_back(components::Text(code->title).font(theme::Font::title2).font_weight(theme::Font::Weight::bold));
        if (auto matrix = app::QrMatrix::encode(code->url))
            rows.push_back(app::arisa::QrCode(std::move(*matrix), codeSide));
        rows.push_back(components::Text(code->message)
                           .font(theme::Font::subheadline)
                           .foreground_style(theme::ColourRole::secondary_label)
                           .frame({ .max_width = codeWidth }));
    }
    rows.push_back(components::Button(strings->text("akira/common/close"), [model] { model->dismissCode(); })
                       .button_style(components::ButtonStyle::glass_prominent())
                       .control_size(components::ControlSize::large)
                       .id(composition::ElementKey{ "cloud.code.close" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::space::lg }, std::move(rows))
        .padding(theme::Space{ 28.0F })
        .id(composition::ElementKey{ "cloud.code" });
}

auto CloudScreen::bundleChoices(const std::optional<CloudModel::Detail>& detail, CloudModel* model,
    const app::Strings* strings) -> composition::Element
{
    std::vector<composition::Element> buttons;
    if (detail)
        for (const Game& choice : detail->bundle)
            buttons.push_back(components::Button(choice.name, [model, choice] { model->choose(choice); }));
    buttons.push_back(components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel,
        [model] { model->dismissChoice(); }));
    return components::Group(std::move(buttons));
}

}
