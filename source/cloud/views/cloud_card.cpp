#include "cloud/views/cloud_card.hpp"
#include "app/view_services.hpp"

#include <borealis/core/i18n.hpp>

#include <string>

#include "accounts/views/profile_setup_view.hpp"
#include "accounts/views/qr_sign_in_view.hpp"
#include "app/views/akira_header.hpp"
#include "app/views/motion.hpp"
#include "app/views/theme.hpp"
#include "cloud/service.hpp"
#include "cloud/views/library_view.hpp"

namespace akira::cloud::views {

using namespace brls::literals;

CloudCard::CloudCard()
{
    cloud::Snapshot snapshot = app::ViewServices::current().cloud.snapshotForActiveProfile();
    const auto& pal = app::views::active();

    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::FLEX_START);
    this->setWidth(174);
    this->setHeight(238);
    this->setPadding(16);
    this->setMarginRight(14);
    this->setMarginBottom(14);
    this->setCornerRadius(14);
    this->setFocusable(true);

    std::string pillText = "akira/cloud/pill_unavailable"_i18n;
    NVGcolor pillColor = pal.textDim;
    std::string subText = snapshot.status.detail;
    bool active = true;

    switch (snapshot.status.availability)
    {
        case cloud::Availability::Ready:
            pillText = "akira/cloud/pill_ready"_i18n;
            pillColor = pal.success;
            subText = brls::getStr("akira/cloud/card_count", snapshot.status.gameCount);
            break;
        case cloud::Availability::CatalogAvailable:
            pillText = "akira/cloud/pill_catalog"_i18n;
            pillColor = pal.warning;
            subText = "akira/cloud/card_eligibility"_i18n;
            break;
        case cloud::Availability::SubscriptionRequired:
            pillText = "akira/cloud/pill_unavailable"_i18n;
            pillColor = pal.danger;
            subText = "akira/cloud/card_subscription"_i18n;
            break;
        case cloud::Availability::Checking:
            pillText = "akira/cloud/pill_checking"_i18n;
            pillColor = pal.accent;
            subText = "akira/cloud/card_checking"_i18n;
            break;
        case cloud::Availability::NeedsPairing:
            pillText = "akira/cloud/pill_pair"_i18n;
            pillColor = pal.accent;
            subText = "akira/cloud/card_link"_i18n;
            break;
        case cloud::Availability::Warning:
        case cloud::Availability::LaunchBlocked:
            pillText = "akira/cloud/pill_limited"_i18n;
            pillColor = pal.warning;
            subText = snapshot.status.canPair ? "akira/cloud/card_relink"_i18n : "akira/cloud/card_limited"_i18n;
            break;
        case cloud::Availability::Error:
            pillText = "akira/cloud/pill_offline"_i18n;
            pillColor = pal.danger;
            subText = "akira/cloud/card_offline"_i18n;
            break;
        case cloud::Availability::Empty:
            pillColor = pal.textDim;
            subText = "akira/cloud/card_none"_i18n;
            active = false;
            break;
        case cloud::Availability::NoProfile:
        default:
            active = false;
            break;
    }

    this->setBackgroundColor(active
        ? app::views::withAlpha(pal.accentStrong, 0x24)
        : pal.surface);
    if (active)
    {
        this->setBorderColor(app::views::withAlpha(pal.accent, 0x55));
        this->setBorderThickness(1);
    }

    auto* glyph = new brls::Label();
    glyph->setText("\xEE\x8A\xBD");
    glyph->setFontSize(44);
    glyph->setTextColor(active ? pal.accent : pal.textDim);
    glyph->setMarginTop(4);
    glyph->setMarginBottom(8);
    glyph->setShrink(0);
    this->addView(glyph);

    auto* name = new brls::Label();
    name->setText("akira/cloud/card_badge"_i18n);
    name->setFontSize(20);
    name->setWidth(142);
    name->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    name->setTextColor(active ? pal.text : pal.textMuted);
    name->setMarginBottom(8);
    name->setShrink(0);
    this->addView(name);

    auto* pill = new brls::Box();
    pill->setAxis(brls::Axis::ROW);
    pill->setCornerRadius(11);
    pill->setPaddingLeft(11);
    pill->setPaddingRight(11);
    pill->setPaddingTop(2);
    pill->setPaddingBottom(3);
    pill->setBackgroundColor(app::views::withAlpha(pillColor, 0x2e));
    pill->setShrink(0);
    auto* pillLabel = new brls::Label();
    pillLabel->setText(pillText);
    pillLabel->setFontSize(16);
    pillLabel->setTextColor(pillColor);
    pill->addView(pillLabel);
    this->addView(pill);

    if (!subText.empty())
    {
        auto* sub = new brls::Label();
        sub->setText(subText);
        sub->setFontSize(14);
        sub->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        sub->setTextColor(pal.textDim);
        sub->setMarginTop(8);
        sub->setWidth(142);
        sub->setShrink(0);
        this->addView(sub);
    }

    this->registerClickAction([snapshot](brls::View*) {
        if (snapshot.status.canPair && snapshot.status.degraded)
        {
            brls::Application::pushActivity(new brls::Activity(new accounts::views::QrSignInView()));
            return true;
        }
        if (snapshot.status.canPair && !snapshot.status.canBrowse)
        {
            brls::Application::pushActivity(new brls::Activity(new accounts::views::ProfileSetupView()));
            return true;
        }

        auto* frame = new brls::AppletFrame(new cloud::views::LibraryView());
        app::views::AkiraHeader::decorate(frame);
        app::views::AkiraHeader::registerTabActions(frame);
        brls::Application::pushActivity(new brls::Activity(frame));
        return true;
    });

    app::views::motion::liftOnFocus(this, focusAnim);
}

}
