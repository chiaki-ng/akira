#include "cloud/views/cloud_shortcuts_rail.hpp"

#include "settings/settings_manager.hpp"
#include "trophies/trophy_manager.hpp"
#include "app/views/motion.hpp"
#include "app/views/theme.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace akira::cloud::views {

using namespace brls::literals;

CloudShortcutsRail::CloudShortcutsRail()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setMarginTop(24);

    titleLabel = new brls::Label();
    titleLabel->setText("akira/cloud/card_title"_i18n);
    titleLabel->setFontSize(24);
    titleLabel->setMarginBottom(22);
    this->addView(titleLabel);

    railRow = new brls::Box();
    railRow->setAxis(brls::Axis::ROW);
    railRow->setAlignItems(brls::AlignItems::FLEX_START);
    railRow->setWidthPercentage(100);
    this->addView(railRow);

    refresh();
}

CloudShortcutsRail::~CloudShortcutsRail()
{
    *alive = false;
}

void CloudShortcutsRail::setLaunchHandler(std::function<void(const Game&)> handler)
{
    launchHandler = std::move(handler);
}

void CloudShortcutsRail::setLeadingCard(std::function<brls::View*()> factory)
{
    leadingCardFactory = std::move(factory);
    refresh();
}

void CloudShortcutsRail::removeShortcut(const std::string& productId)
{
    auto* settings = SettingsManager::getInstance();
    std::vector<Game> games = settings->getCloudShortcuts();
    games.erase(std::remove_if(games.begin(), games.end(),
                    [&](const Game& g) { return g.productId == productId; }),
        games.end());
    settings->setCloudShortcuts(games);
    settings->writeFile();
    refresh();
}

void CloudShortcutsRail::refresh()
{
    brls::View* prevFocus = brls::Application::getCurrentFocus();
    bool hadFocus = false;
    for (brls::View* v = prevFocus; v; v = v->getParent())
        if (v == this) { hadFocus = true; break; }

    railRow->clearViews();
    const int myGen = ++refreshGen;

    std::vector<Game> games = SettingsManager::getInstance()->getCloudShortcuts();
    bool hasProfile = !SettingsManager::getInstance()->getProfiles().empty();
    brls::View* lead = (hasProfile && leadingCardFactory) ? leadingCardFactory() : nullptr;
    if (!lead && games.empty())
    {
        this->setVisibility(brls::Visibility::GONE);
        if (hadFocus && this->getParent())
            brls::Application::giveFocus(this->getParent());
        return;
    }
    this->setVisibility(brls::Visibility::VISIBLE);

    if (lead)
    {
        lead->setId("cloud/rail/lead");
        lead->setCustomNavigationRoute(brls::FocusDirection::UP, "host/container");
        lead->setMarginRight(16);
        railRow->addView(lead);
    }

    auto guard = alive;
    auto fetchInto = [this, guard, myGen](brls::Image* cover, const std::string& url) {
        if (url.empty())
            return;
        auto inner = guard;
        trophies::TrophyManager::getInstance()->fetchIcon(url,
            [this, inner, cover, myGen](const std::string&, const std::vector<uint8_t>& bytes) {
                if (!*inner || myGen != refreshGen)
                    return;
                if (!bytes.empty())
                    cover->setImageFromMem(bytes.data(), static_cast<int>(bytes.size()));
            });
    };

    size_t count = std::min(static_cast<size_t>(6), games.size());
    for (size_t i = 0; i < count; i++)
    {
        const Game& game = games[i];

        auto* card = new app::views::motion::LiftBox();
        card->setAxis(brls::Axis::COLUMN);
        card->setWidth(174.0f);
        card->setHeight(238.0f);
        if (i + 1 < count)
            card->setMarginRight(16);
        card->setCornerRadius(14);
        card->setBackgroundColor(app::views::active().surface);
        card->setPadding(12.0f, 12.0f, 12.0f, 12.0f);
        card->setFocusable(true);
        card->setCustomNavigationRoute(brls::FocusDirection::UP, "host/container");

        Game gameCopy = game;
        card->registerClickAction([this, gameCopy](brls::View*) {
            if (launchHandler)
                launchHandler(gameCopy);
            return true;
        });

        std::string pid = game.productId;
        card->registerAction("akira/hosts/shortcut_remove"_i18n, brls::ControllerButton::BUTTON_X,
            [this, pid](brls::View*) {
                removeShortcut(pid);
                return true;
            },
            false);

        auto* cover = new brls::Image();
        cover->setWidthPercentage(100.0f);
        cover->setHeight(172.0f);
        cover->setCornerRadius(10);
        cover->setScalingType(brls::ImageScalingType::FILL);
        cover->setBackgroundColor(app::views::active().surfaceElevated);
        card->addView(cover);

        auto* name = new brls::Label();
        name->setText(game.name);
        name->setFontSize(22);
        name->setMarginTop(10);
        name->setWidthPercentage(100.0f);
        name->setSingleLine(true);
        name->setAutoAnimate(true);
        name->setTextColor(app::views::active().text);
        card->addView(name);

        railRow->addView(card);
        fetchInto(cover, game.imageUrl.empty() ? game.artworkUrl() : game.imageUrl);
    }

    if (hadFocus)
    {
        const auto& cards = railRow->getChildren();
        if (!cards.empty())
            brls::Application::giveFocus(cards.front());
    }
}

}
