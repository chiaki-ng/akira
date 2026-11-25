#include "accounts/ui/account_flow.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/colour_value.hpp>

#include <chrono>
#include <vector>

#include "accounts/ui/companion_pairing.hpp"
#include "accounts/ui/legacy_profile.hpp"
#include "accounts/ui/qr_sign_in.hpp"
#include "accounts/ui/welcome.hpp"
#include "app/arisa/ambient_background.hpp"

namespace akira::accounts::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

AccountFlow::AccountFlow(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto AccountFlow::backdrop() -> composition::Element
{
    const graphics::Gradient glow(std::vector<theme::ColourValue>{
        theme::ColourValue(theme::ColourRole::accent).opacity(glowOpacity),
        theme::ColourValue(theme::ColourRole::accent).opacity(0.0F) });
    return composition::Element(components::Group())
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(graphics::ShapeStyle(graphics::EllipticalGradient{ .gradient = glow,
            .center = motion::UnitPoint{ 0.5F, 0.35F },
            .end_radius_fraction = 0.7F }))
        .background(theme::ColourRole::window_background);
}

auto AccountFlow::ticking(AccountModel* model) -> async::Task<void>
{
    while (true)
    {
        model->tick(AccountModel::Clock::now());
        co_await async::sleep(std::chrono::seconds{ 1 });
    }
}

auto AccountFlow::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    composition::Element shown = Welcome(*model, *strings);
    switch (view.flow)
    {
        case AccountModel::Flow::Qr: shown = QrSignIn(*model, *strings); break;
        case AccountModel::Flow::Companion: shown = CompanionPairing(*model, *strings); break;
        case AccountModel::Flow::Legacy: shown = LegacyProfile(*model, *strings); break;
        case AccountModel::Flow::None:
        case AccountModel::Flow::Welcome: break;
    }
    AccountModel* held = model;
    return components::ZStack(backdrop(),
        composition::Element(app::arisa::AmbientBackground(app::arisa::AmbientBackground::Presence::Quiet))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity }),
        std::move(shown))
        .task([held] { return ticking(held); })
        .id(composition::ElementKey{ "account-flow" });
}

}
