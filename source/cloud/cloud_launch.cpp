#include "cloud/cloud_launch.hpp"

#include "play/chiaki_core.hpp"

#include <utility>

namespace akira::cloud {

namespace catalog = libchiaking::catalog;
namespace play = libchiaking::play;

CloudLaunch::CloudLaunch(std::shared_ptr<libchiaking::users::PsUser> player, catalog::ClearedLaunch cleared,
                         std::string name)
    : user(std::move(player)), cleared(std::move(cleared)), title(std::move(name))
{
}

auto CloudLaunch::start(akira::play::ChiakiCore& core, play::Preferences preferences, libchiaking::media::MediaSink& media,
                        play::SessionEvents& events) const -> Started
{
    return core.chiaki().play().start_on_cloud(
        user, cleared.product, std::move(preferences), play::DatacenterPreference{}, media, events, cleared);
}

auto CloudLaunch::clearanceText(const app::Strings& strings, const catalog::LaunchRefusal& refusal,
                                const std::string& name) -> std::string
{
    using Kind = catalog::LaunchRefusal::Kind;
    switch (refusal.kind())
    {
        case Kind::add_to_library:
            return strings.format("akira/cloud/launch_not_in_library", name);
        case Kind::needs_premium:
            return strings.text("akira/cloud/launch_ps_plus");
        case Kind::unannounced:
        case Kind::not_released:
        case Kind::no_trusted_clock:
            return strings.text("akira/cloud/launch_not_released");
        case Kind::check_unavailable:
            return strings.text("akira/cloud/launch_network");
        case Kind::not_listed:
        case Kind::hidden:
        case Kind::buy_to_stream:
        case Kind::refused_by_service:
        case Kind::not_streamable:
        case Kind::invalid_title:
        case Kind::title_taken_down:
        case Kind::streaming_unavailable:
        case Kind::purchase_needed:
            return strings.text("akira/cloud/launch_unavailable");
        case Kind::unexpected_entitlement_state:
        case Kind::choose_a_game:
            break;
    }
    return strings.text("akira/cloud/launch_failed");
}

auto CloudLaunch::refusalText(const app::Strings& strings, const play::LaunchRefused& refused,
                              const std::string& name) -> std::string
{
    using Kind = play::LaunchRefused::Kind;
    switch (refused.kind())
    {
        case Kind::not_cleared:
            return refused.clearance() ? clearanceText(strings, *refused.clearance(), name) : strings.text("akira/cloud/launch_failed");
        case Kind::privacy_settings:
        {
            std::string text = strings.text("akira/cloud/launch_privacy");
            if (!refused.upgrade_url().empty())
                text += "\n\n" + std::string(refused.upgrade_url());
            return text + "\n\n" + strings.text("akira/cloud/launch_privacy_action");
        }
        case Kind::membership_rejected:
        case Kind::outside_streaming_plan:
            return strings.text("akira/cloud/launch_ps_plus");
        case Kind::licence_not_free:
        case Kind::title_not_found:
        case Kind::refused_by_service:
            return strings.text("akira/cloud/launch_unavailable");
        case Kind::no_reachable_datacenter:
            return strings.text("akira/cloud/launch_network");
        case Kind::datacenter_unavailable:
            return strings.format("akira/cloud/launch_datacenter_unavailable",
                refused.datacenter() ? refused.datacenter()->text() : std::string());
        case Kind::no_server:
            return strings.text("akira/cloud/launch_busy");
        case Kind::sign_in_expired:
            return strings.text("akira/cloud/status_relink_detail");
        case Kind::store_region:
            return strings.format("akira/cloud/launch_store_region", name);
        case Kind::not_built:
            return strings.text("akira/cloud/launch_not_built");
        case Kind::unavailable:
            break;
    }
    return strings.text("akira/cloud/launch_failed");
}

} // namespace akira::cloud
