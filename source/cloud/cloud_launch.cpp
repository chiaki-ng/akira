#include "cloud/cloud_launch.hpp"

#include "core/chiaki_core.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include <utility>

using namespace brls::literals;

namespace cloud {

namespace catalog = libchiaking::catalog;
namespace play = libchiaking::play;

CloudLaunch::CloudLaunch(std::shared_ptr<libchiaking::users::PsUser> player, catalog::ClearedLaunch cleared,
                         std::string name)
    : user(std::move(player)), cleared(std::move(cleared)), title(std::move(name))
{
}

auto CloudLaunch::start(play::Preferences preferences, libchiaking::media::MediaSink& media, play::SessionEvents& events) const
    -> Started
{
    return akira::ChiakiCore::current().chiaki().play().start_on_cloud(
        user, cleared.product, std::move(preferences), play::DatacenterPreference{}, media, events, cleared);
}

auto CloudLaunch::clearanceText(const catalog::LaunchRefusal& refusal, const std::string& name) -> std::string
{
    using Kind = catalog::LaunchRefusal::Kind;
    switch (refusal.kind())
    {
        case Kind::add_to_library:
            return brls::getStr("akira/cloud/launch_not_in_library", name);
        case Kind::needs_premium:
            return "akira/cloud/launch_ps_plus"_i18n;
        case Kind::unannounced:
        case Kind::not_released:
        case Kind::no_trusted_clock:
            return "akira/cloud/launch_not_released"_i18n;
        case Kind::check_unavailable:
            return "akira/cloud/launch_network"_i18n;
        case Kind::not_listed:
        case Kind::hidden:
        case Kind::buy_to_stream:
        case Kind::refused_by_service:
        case Kind::not_streamable:
        case Kind::invalid_title:
        case Kind::title_taken_down:
        case Kind::streaming_unavailable:
        case Kind::purchase_needed:
            return "akira/cloud/launch_unavailable"_i18n;
        case Kind::unexpected_entitlement_state:
        case Kind::choose_a_game:
            break;
    }
    return "akira/cloud/launch_failed"_i18n;
}

auto CloudLaunch::refusalText(const play::LaunchRefused& refused, const std::string& name) -> std::string
{
    using Kind = play::LaunchRefused::Kind;
    switch (refused.kind())
    {
        case Kind::not_cleared:
            return refused.clearance() ? clearanceText(*refused.clearance(), name) : "akira/cloud/launch_failed"_i18n;
        case Kind::privacy_settings:
        {
            std::string text = "akira/cloud/launch_privacy"_i18n;
            if (!refused.upgrade_url().empty())
                text += "\n\n" + std::string(refused.upgrade_url());
            return text + "\n\n" + "akira/cloud/launch_privacy_action"_i18n;
        }
        case Kind::membership_rejected:
        case Kind::outside_streaming_plan:
            return "akira/cloud/launch_ps_plus"_i18n;
        case Kind::licence_not_free:
        case Kind::title_not_found:
        case Kind::refused_by_service:
            return "akira/cloud/launch_unavailable"_i18n;
        case Kind::no_reachable_datacenter:
            return "akira/cloud/launch_network"_i18n;
        case Kind::datacenter_unavailable:
            return brls::getStr("akira/cloud/launch_datacenter_unavailable",
                refused.datacenter() ? refused.datacenter()->text() : std::string());
        case Kind::no_server:
            return "akira/cloud/launch_busy"_i18n;
        case Kind::sign_in_expired:
            return "akira/cloud/status_relink_detail"_i18n;
        case Kind::store_region:
            return brls::getStr("akira/cloud/launch_store_region", name);
        case Kind::not_built:
            return "akira/cloud/launch_not_built"_i18n;
        case Kind::unavailable:
            break;
    }
    return "akira/cloud/launch_failed"_i18n;
}

} // namespace cloud
