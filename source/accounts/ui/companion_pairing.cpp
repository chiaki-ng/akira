#include "accounts/ui/companion_pairing.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

CompanionPairing::CompanionPairing(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto CompanionPairing::failed(AccountDesk::PairStep step) -> bool
{
    switch (step)
    {
        case AccountDesk::PairStep::BadCode:
        case AccountDesk::PairStep::LockedOut:
        case AccountDesk::PairStep::TimedOut:
        case AccountDesk::PairStep::Error: return true;
        case AccountDesk::PairStep::Waiting:
        case AccountDesk::PairStep::Connecting:
        case AccountDesk::PairStep::Imported: break;
    }
    return false;
}

auto CompanionPairing::digits(const std::string& code) -> composition::Element
{
    std::vector<composition::Element> boxes;
    for (const char digit : code)
        boxes.push_back(components::Text(std::string(1, digit))
                            .font(theme::Font::system(40.0F, theme::Font::Weight::semibold))
                            .frame({ .width = digitWidth, .height = digitHeight })
                            .glass_panel(components::PanelRadius::medium));
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } }, std::move(boxes))
        .id(composition::ElementKey{ "companion.code" });
}

auto CompanionPairing::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    const AccountModel::Companion& companion = view.companion;
    AccountModel* held = model;
    const bool problem = failed(companion.step);
    std::vector<composition::Element> status;
    if (problem)
        status.push_back(components::Image(components::Image::SystemName{ "error" }).foreground_style(theme::ColourRole::red));
    else
        status.push_back(controls::ProgressView());
    status.push_back(components::Text(strings->text(AccountModel::pairStatusKey(companion.step)))
                         .font(theme::Font::callout)
                         .foreground_style(problem ? theme::ColourValue(theme::ColourRole::red)
                                                   : theme::ColourValue(theme::ColourRole::secondary_label)));
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/pair/title")).font(theme::Font::title).font_weight(theme::Font::Weight::semibold));
    rows.push_back(components::Text(strings->text("akira/pair/instructions")).font(theme::Font::body));
    rows.push_back(digits(companion.pairing.code));
    rows.push_back(components::Text(companion.pairing.address.empty()
                                        ? strings->format("akira/pair/addr_port", companion.pairing.port)
                                        : strings->format("akira/pair/addr_full", companion.pairing.address, companion.pairing.port))
                       .font(theme::Font::callout)
                       .foreground_style(theme::ColourRole::accent)
                       .id(composition::ElementKey{ "companion.address" }));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(status))
                       .id(composition::ElementKey{ "companion.status" }));
    if (companion.secondsLeft >= 0)
        rows.push_back(components::Text(strings->format("akira/pair/timer_fmt", AccountModel::countdown(companion.secondsLeft)))
                           .font(theme::Font::footnote)
                           .foreground_style(theme::ColourRole::tertiary_label)
                           .id(composition::ElementKey{ "companion.timer" }));
    rows.push_back(components::HStack(components::Spacer(),
        components::Button(strings->text("akira/common/back"), components::ButtonRole::cancel, [held] { held->back(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .id(composition::ElementKey{ "companion.back" })));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 18.0F } },
        std::move(rows))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::large)
        .on_exit_command(strings->text("akira/common/back"), [held] { held->back(); })
        .id(composition::ElementKey{ "companion" });
}

}
