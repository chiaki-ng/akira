#include "views/profile_chip_view.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "ui/theme.hpp"
#include "core/trophy_manager.hpp"

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

ProfileChipView::ProfileChipView() {
    this->inflateFromXMLRes("xml/profile_chip.xml");
    settings = SettingsManager::getInstance();

    const auto& pal = akira::ui::active();

    this->setAlignItems(brls::AlignItems::CENTER);
    this->setBackgroundColor(brls::Application::getTheme()["color/card"]);

    avatar->setScalingType(brls::ImageScalingType::FILL);
    avatar->setBackgroundColor(pal.surfaceElevated);
    avatar->setCornerRadius(20);

    nameLabel->setTextColor(pal.text);
    plusBadge->setBackgroundColor(akira::ui::withAlpha(pal.gold, 0x33));
    plusLabel->setTextColor(pal.gold);
    tagBox->setBorderColor(pal.surfaceLine);
    tagLabel->setTextColor(pal.textMuted);
    cloudLabel->setTextColor(pal.textMuted);

    refresh();
}

ProfileChipView::~ProfileChipView() {
    *alive = false;
}

void ProfileChipView::refresh() {
#if AKIRA_CLOUD
    const auto& pal = akira::ui::active();
#endif
    const Profile* p = settings->getActiveProfile();

    std::string name;
    if (!p)
        name = "akira/settings/no_active_profile"_i18n;
    else if (p->label().empty())
        name = "akira/settings/unnamed_profile"_i18n;
    else
        name = settings->maskAccountName(p->label());
    nameLabel->setText(name);

    /*
     * Legacy has no cloud and no PSN-brokered remote, so the tag and the cloud line have
     * nothing to say. The chip collapses to the profile name.
     */
    const bool legacy = p && p->legacy;

    if (legacy) {
        tagLabel->setVisibility(brls::Visibility::GONE);
        cloudLabel->setVisibility(brls::Visibility::GONE);
        dot->setVisibility(brls::Visibility::GONE);
        plusBadge->setVisibility(brls::Visibility::GONE);
        return;
    }

    tagLabel->setVisibility(brls::Visibility::VISIBLE);
#if AKIRA_CLOUD
    cloudLabel->setVisibility(brls::Visibility::VISIBLE);
    dot->setVisibility(brls::Visibility::VISIBLE);
#else
    cloudLabel->setVisibility(brls::Visibility::GONE);
    dot->setVisibility(brls::Visibility::GONE);
#endif

    tagLabel->setText(p && p->isRemote()
        ? "akira/settings/profile_remote"_i18n
        : "akira/settings/profile_local"_i18n);

#if AKIRA_CLOUD
    cloud::Status status = p
        ? cloud::Service::instance().snapshotForActiveProfile().status
        : cloud::Status{};
    std::string cloudText = cloudSummary(status);
    if (cloudText.empty())
        cloudText = "akira/cloud/chip_pair"_i18n;
    cloudLabel->setText(cloudText);

    NVGcolor dotColor;
    switch (status.availability)
    {
        case cloud::Availability::Ready: dotColor = pal.success; break;
        case cloud::Availability::Checking: dotColor = pal.accent; break;
        case cloud::Availability::CatalogAvailable:
        case cloud::Availability::Warning:
        case cloud::Availability::LaunchBlocked: dotColor = pal.warning; break;
        case cloud::Availability::SubscriptionRequired: dotColor = pal.danger; break;
        case cloud::Availability::Error: dotColor = pal.danger; break;
        default: dotColor = pal.textDim; break;
    }
    dot->setBackgroundColor(dotColor);
#endif

    plusBadge->setVisibility(brls::Visibility::GONE);

    if (!p)
        return;

    auto guard = alive;

    TrophyManager::getInstance()->fetchProfile(false,
        [this, guard](const TrophyManager::Profile& prof) {
            if (!*guard)
                return;
            if (!prof.online_id.empty())
                nameLabel->setText(settings->maskAccountName(prof.online_id));
            plusBadge->setVisibility(prof.membership.is_ps_plus_member().value_or(false) ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

            std::string url = prof.avatar_url.value_or("");
            if (url.empty())
                return;

            auto inner = guard;
            TrophyManager::getInstance()->fetchIcon(url,
                [this, inner](const std::string&, const std::vector<uint8_t>& bytes) {
                    if (!*inner)
                        return;
                    if (!bytes.empty())
                        avatar->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
                });
        },
        [](psn::Status, const std::string&) {});
}
