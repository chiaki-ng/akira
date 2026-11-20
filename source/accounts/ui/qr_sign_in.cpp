#include "accounts/ui/qr_sign_in.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

#include "app/arisa/qr_code.hpp"

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

QrSignIn::QrSignIn(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto QrSignIn::code(const AccountModel::Qr& qr) const -> composition::Element
{
    if (qr.code)
        return composition::Element(app::arisa::QrCode(*qr.code, codeSide)).id(composition::ElementKey{ "qr-sign-in.code" });
    composition::Element placeholder = qr.step == AccountDesk::QrStep::Failed
        ? composition::Element(components::Text(strings->text("akira/qr_sign_in/placeholder_failed"))
                                   .foreground_style(theme::ColourRole::secondary_label))
        : composition::Element(controls::ProgressView());
    return std::move(placeholder)
        .frame({ .width = codeSide, .height = codeSide })
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.07F), theme::Shape::rounded(app::arisa::QrCode::radius))
        .id(composition::ElementKey{ "qr-sign-in.placeholder" });
}

auto QrSignIn::status(const AccountModel::Qr& qr) const -> composition::Element
{
    const bool failed = qr.step == AccountDesk::QrStep::Failed;
    const std::string text = failed && !qr.failureKey.empty() ? strings->text(qr.failureKey)
                                                              : strings->text(AccountModel::qrStatusKey(qr.step));
    std::vector<composition::Element> parts;
    if (failed)
        parts.push_back(components::Image(components::Image::SystemName{ "error" }).foreground_style(theme::ColourRole::red));
    else
        parts.push_back(controls::ProgressView());
    parts.push_back(components::Text(text)
                        .font(theme::Font::callout)
                        .foreground_style(failed ? theme::ColourValue(theme::ColourRole::red)
                                                 : theme::ColourValue(theme::ColourRole::secondary_label)));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(parts))
        .id(composition::ElementKey{ "qr-sign-in.status" });
}

auto QrSignIn::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    const AccountModel::Qr& qr = view.qr;
    AccountModel* held = model;
    std::vector<composition::Element> side;
    side.push_back(components::Text(strings->text("akira/qr_sign_in/step_scan")).font(theme::Font::body));
    side.push_back(components::Text(strings->text("akira/qr_sign_in/step_approve")).font(theme::Font::body));
    side.push_back(status(qr));
    if (qr.secondsLeft >= 0)
        side.push_back(components::Text(strings->format("akira/qr_sign_in/timer_fmt", AccountModel::countdown(qr.secondsLeft)))
                           .font(theme::Font::footnote)
                           .foreground_style(theme::ColourRole::tertiary_label)
                           .id(composition::ElementKey{ "qr-sign-in.timer" }));
    std::vector<composition::Element> actions;
    actions.push_back(components::Spacer());
    actions.push_back(components::Button(strings->text("akira/common/back"), components::ButtonRole::cancel, [held] { held->back(); })
                          .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                          .id(composition::ElementKey{ "qr-sign-in.back" }));
    actions.push_back(components::Button(strings->text(qr.step == AccountDesk::QrStep::Failed ? "akira/qr_sign_in/retry"
                                                                                              : "akira/qr_sign_in/new_code"),
        components::Image::SystemName{ "refresh" }, [held] { held->retryQr(); })
                          .button_style(qr.step == AccountDesk::QrStep::Failed ? components::ButtonStyle::glass_prominent()
                                                                               : components::ButtonStyle::glass(theme::Glass::clear()))
                          .id(composition::ElementKey{ "qr-sign-in.retry" }));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 22.0F } },
        components::Text(strings->text("akira/qr_sign_in/title")).font(theme::Font::title).font_weight(theme::Font::Weight::semibold),
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::Space{ 32.0F } },
            code(qr),
            components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::Space{ 14.0F } },
                std::move(side))),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(actions)))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = width })
        .glass_panel(components::PanelRadius::large)
        .on_exit_command(strings->text("akira/common/back"), [held] { held->back(); })
        .id(composition::ElementKey{ "qr-sign-in" });
}

}
