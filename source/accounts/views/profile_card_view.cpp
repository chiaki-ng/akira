#include "accounts/views/profile_card_view.hpp"
#include "app/view_services.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "app/views/theme.hpp"
#include "trophies/trophy_manager.hpp"

#include <algorithm>
#include <format>

namespace akira::accounts::views {

using namespace brls::literals;

#if AKIRA_CLOUD
namespace {

std::string cloudSummary(const cloud::Status& status)
{
    switch (status.availability)
    {
        case cloud::Availability::Ready:
            return "akira/cloud/chip_ready"_i18n;
        case cloud::Availability::CatalogAvailable:
            return "akira/cloud/chip_catalog"_i18n;
        case cloud::Availability::SubscriptionRequired:
            return "akira/cloud/chip_unavailable"_i18n;
        case cloud::Availability::Warning:
            return "akira/cloud/chip_warning"_i18n;
        case cloud::Availability::Empty:
            return "akira/cloud/chip_empty"_i18n;
        case cloud::Availability::NeedsPairing:
            return "akira/cloud/chip_pair"_i18n;
        case cloud::Availability::Checking:
            return "akira/cloud/chip_checking"_i18n;
        case cloud::Availability::LaunchBlocked:
        case cloud::Availability::Error:
            return "akira/cloud/chip_error"_i18n;
        case cloud::Availability::NoProfile:
        default:
            return "";
    }
}

} // namespace
#endif

ProfileCardView::ProfileCardView() {
    this->inflateFromXMLRes("xml/profile_card.xml");
    settings = settings::SettingsManager::getInstance();

    this->setAlignItems(brls::AlignItems::CENTER);
    this->setBackgroundColor(app::views::active().surface);

    avatar->setScalingType(brls::ImageScalingType::FILL);
    avatar->setBackgroundColor(app::views::active().surfaceElevated);
    avatar->setCornerRadius(50);

    track->setBackgroundColor(app::views::active().surfaceLine);
    progress->setColor(app::views::active().accentStrong);
    progress->setCornerRadius(3);

    plusLabel->setTextColor(app::views::active().warning);
    statusLabel->setTextColor(app::views::active().textMuted);
    levelLabel->setTextColor(app::views::active().textMuted);
    platLabel->setTextColor(app::views::active().text);
    goldLabel->setTextColor(app::views::active().text);
    silverLabel->setTextColor(app::views::active().text);
    bronzeLabel->setTextColor(app::views::active().text);

    platImg->setImageFromRes("img/trophy/platinum.png");
    goldImg->setImageFromRes("img/trophy/gold.png");
    silverImg->setImageFromRes("img/trophy/silver.png");
    bronzeImg->setImageFromRes("img/trophy/bronze.png");
    platImg->setScalingType(brls::ImageScalingType::FIT);
    goldImg->setScalingType(brls::ImageScalingType::FIT);
    silverImg->setScalingType(brls::ImageScalingType::FIT);
    bronzeImg->setScalingType(brls::ImageScalingType::FIT);

    refresh();
}

ProfileCardView::~ProfileCardView() {
    *alive = false;
}

void ProfileCardView::refresh() {
    const Profile* p = settings->accounts().activeProfile();

    std::string display;
    if (!p)
        display = "akira/settings/no_active_profile"_i18n;
    else if (p->label().empty())
        display = "akira/settings/unnamed_profile"_i18n;
    else
        display = settings->app().maskAccountName(p->label());
    onlineIdLabel->setText(display);

    std::string status = p && p->isRemote()
        ? "akira/settings/profile_remote"_i18n
        : "akira/settings/profile_local"_i18n;
#if AKIRA_CLOUD
    if (p && !p->legacy)
    {
        std::string cloud = cloudSummary(cloud::Service::instance().snapshotForActiveProfile().status);
        if (!cloud.empty())
            status += "  ·  " + cloud;
    }
#endif
    if (p && p->legacy)
        status = "akira/settings/profile_legacy"_i18n;
    statusLabel->setText(status);
    plusLabel->setVisibility(brls::Visibility::GONE);
    trophyCol->setVisibility(brls::Visibility::GONE);
    levelLabel->setText("");
    progress->setWidthPercentage(0.0f);
    platLabel->setText("");
    goldLabel->setText("");
    silverLabel->setText("");
    bronzeLabel->setText("");

    if (!p)
        return;

    /* No token, so no profile lookup and no avatar to fetch. */
    if (p->legacy)
        return;

    auto guard = alive;

    app::ViewServices::current().trophies.fetchProfile(false,
        [this, guard](const trophies::TrophyManager::Profile& prof) {
            if (!*guard)
                return;
            if (!prof.online_id.empty())
                onlineIdLabel->setText(settings->app().maskAccountName(prof.online_id));
            plusLabel->setVisibility(prof.membership.is_ps_plus_member().value_or(false) ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

            std::string url = prof.avatar_url.value_or("");
            if (url.empty())
                return;

            auto inner = guard;
            app::ViewServices::current().trophies.fetchIcon(url,
                [this, inner](const std::string&, const std::vector<uint8_t>& bytes) {
                    if (!*inner)
                        return;
                    if (!bytes.empty())
                        avatar->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
                });
        },
        [](Status, const std::string&) {});

    app::ViewServices::current().trophies.fetchSummary(false,
        [this, guard](const trophies::TrophyManager::Summary& summary) {
            if (!*guard)
                return;
            trophyCol->setVisibility(brls::Visibility::VISIBLE);
            levelLabel->setText(std::format("{} {} · {}%",
                "akira/settings/trophy_level"_i18n, summary.level, summary.progress));

            int pct = std::min(100, static_cast<int>(summary.progress));
            progress->setWidthPercentage(static_cast<float>(pct));

            platLabel->setText(std::to_string(summary.earned.platinum));
            goldLabel->setText(std::to_string(summary.earned.gold));
            silverLabel->setText(std::to_string(summary.earned.silver));
            bronzeLabel->setText(std::to_string(summary.earned.bronze));

            platImg->setAlpha(summary.earned.platinum > 0 ? 1.0f : 0.4f);
            goldImg->setAlpha(summary.earned.gold > 0 ? 1.0f : 0.4f);
            silverImg->setAlpha(summary.earned.silver > 0 ? 1.0f : 0.4f);
            bronzeImg->setAlpha(summary.earned.bronze > 0 ? 1.0f : 0.4f);
        },
        [](Status, const std::string&) {});
}

}
