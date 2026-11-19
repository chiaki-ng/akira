#include "cloud/views/cloud_game_card.hpp"
#include "app/view_services.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>

#include "app/views/motion.hpp"
#include "app/views/theme.hpp"
#include "trophies/trophy_manager.hpp"

namespace akira::cloud::views {

using namespace brls::literals;

brls::Box* CloudGameCard::makePill(const std::string& text, NVGcolor color)
{
    auto* pill = new brls::Box();
    pill->setAxis(brls::Axis::ROW);
    pill->setCornerRadius(9);
    pill->setPaddingLeft(9);
    pill->setPaddingRight(9);
    pill->setPaddingTop(1);
    pill->setPaddingBottom(2);
    pill->setMarginRight(6);
    pill->setBackgroundColor(app::views::withAlpha(color, 0x2e));

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(14);
    label->setTextColor(color);
    pill->addView(label);
    return pill;
}

CloudGameCard::CloudGameCard()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setFocusable(false);
    this->setCornerRadius(14);
    this->setPadding(12, 12, 12, 12);
    this->setBackgroundColor(app::views::active().surface);

    cover = new brls::Image();
    cover->setHeight(178);
    cover->setWidthPercentage(100.0f);
    cover->setCornerRadius(10);
    cover->setScalingType(brls::ImageScalingType::FILL);
    cover->setBackgroundColor(app::views::active().surfaceElevated);
    this->addView(cover);

    name = new brls::Label();
    name->setFontSize(20);
    name->setMarginTop(10);
    name->setWidthPercentage(100.0f);
    name->setSingleLine(true);
    this->addView(name);

    pillRow = new brls::Box();
    pillRow->setAxis(brls::Axis::ROW);
    pillRow->setMarginTop(8);
    this->addView(pillRow);

    this->registerClickAction([this](brls::View*) {
        if (boundGame.launchLeftOut)
            brls::Application::notify("akira/cloud/launch_not_built"_i18n);
        else if (launchCb)
            launchCb(boundGame);
        return true;
    });

    this->registerAction("akira/cloud/pin_shortcut"_i18n, brls::ControllerButton::BUTTON_X,
        [this](brls::View*) {
            if (boundGame.launchable() && pinCb)
                pinCb(boundGame);
            return true;
        });

    app::views::motion::liftOnFocus(this, focusAnim);
    live.push_back(this);
}

CloudGameCard::~CloudGameCard()
{
    live.erase(
        std::remove(live.begin(), live.end(), this),
        live.end());
}

void CloudGameCard::bind(const Game& game, const std::function<void(const Game&)>& onLaunch,
    const std::function<void(const Game&)>& onPin)
{
    const auto& pal = app::views::active();
    const GameBadges badges(game);
    bool streamable = badges.playable();

    boundGame = game;
    launchCb = onLaunch;
    pinCb = onPin;

    this->setVisibility(brls::Visibility::VISIBLE);
    this->setFocusable(true);

    name->setText(game.name);
    name->setTextColor(streamable ? pal.text : pal.textMuted);
    cover->setAlpha(streamable ? 1.0f : 0.5f);
    cover->clear();

    pillRow->setAlpha(badges.leftOut() ? 0.5f : 1.0f);
    pillRow->clearViews();
    for (const GameBadges::Badge& badge : badges.badges())
        pillRow->addView(pillFor(badge));

    iconUrl = game.artworkUrl();
    if (iconUrl.empty())
        return;

    app::ViewServices::current().trophies.fetchIcon(iconUrl,
        [](const std::string& url, const std::vector<uint8_t>& bytes) {
            if (bytes.empty())
                return;
            for (CloudGameCard* card : live)
                if (card->iconUrl == url)
                    card->cover->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
        });
}

brls::Box* CloudGameCard::pillFor(const GameBadges::Badge& badge)
{
    const auto& pal = app::views::active();
    switch (badge.kind)
    {
        case GameBadges::Kind::PlatformPs5: return makePill(badge.label, pal.accentStrong);
        case GameBadges::Kind::PlatformPs4: return makePill(badge.label, pal.textMuted);
        case GameBadges::Kind::PlatformOther: return makePill(badge.label, pal.media);
        case GameBadges::Kind::Plus: return makePill("akira/cloud/badge_plus"_i18n, pal.gold);
        case GameBadges::Kind::Verified: return makePill("akira/cloud/badge_verified"_i18n, pal.success);
        case GameBadges::Kind::Unavailable: return makePill("akira/cloud/badge_unavailable"_i18n, pal.danger);
        case GameBadges::Kind::Unverified: return makePill("akira/cloud/badge_unverified"_i18n, pal.textMuted);
        case GameBadges::Kind::CategoryOwned: return makePill(badge.label, pal.success);
        case GameBadges::Kind::CategoryCatalog: return makePill(badge.label, pal.media);
        case GameBadges::Kind::CategoryOther: break;
    }
    return makePill(badge.label, pal.warning);
}

void CloudGameCard::reset()
{
    this->setVisibility(brls::Visibility::INVISIBLE);
    this->setFocusable(false);
    launchCb = nullptr;
    iconUrl.clear();
    cover->clear();
    pillRow->clearViews();
}

}
