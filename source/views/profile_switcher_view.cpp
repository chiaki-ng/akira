#include "views/profile_switcher_view.hpp"
#include "views/host_list_tab.hpp"
#include "views/pair_view.hpp"
#include "views/profile_setup_view.hpp"
#include "ui/theme.hpp"
#include "ui/motion.hpp"
#include "core/chiaki_core.hpp"
#include "views/qr_sign_in_view.hpp"
#include "core/trophy_manager.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>

using namespace brls::literals;

namespace {
constexpr float ROW_H = 58.0f;
constexpr float SEP_H = 1.0f;
constexpr float AVATAR = 38.0f;

const char* ICON_EXPAND_MORE = "";
const char* ICON_EXPAND_LESS = "";
const char* ICON_CHECK       = "";
const char* ICON_DELETE      = "";
const char* ICON_ADD         = "";

NVGcolor transparent() { return nvgRGBA(0, 0, 0, 0); }

/*
 * popActivity only erases from the stack in its hide-animation callback, so a
 * while (size > 1) loop never terminates. Chain through the callback, one frame apart.
 */
void popToHome() {
    if (brls::Application::getActivitiesStack().size() <= 1)
        return;

    brls::Application::popActivity(brls::TransitionAnimation::FADE, []() {
        brls::sync([]() { popToHome(); });
    });
}
}

ProfileSwitcherView::ProfileSwitcherView() {
    settings = SettingsManager::getInstance();

    const auto& palette = akira::ui::active();

    this->setAxis(brls::Axis::COLUMN);
    this->setBackgroundColor(palette.surface);
    this->setCornerRadius(14.0f);
    this->setClipsToBounds(true);
    this->setMarginBottom(25.0f);

    header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setFocusable(true);
    header->setCornerRadius(14.0f);
    header->registerClickAction([this](brls::View*) {
        setExpanded(!expanded);
        return true;
    });

    card = new ProfileCardView();
    card->setBackgroundColor(transparent());
    card->setCornerRadius(0.0f);
    card->setMarginBottom(0.0f);
    card->setGrow(1.0f);
    header->addView(card);

    chevron = new brls::Label();
    chevron->setText(ICON_EXPAND_MORE);
    chevron->setFontSize(30.0f);
    chevron->setTextColor(palette.accent);
    chevron->setMarginLeft(4.0f);
    chevron->setMarginRight(22.0f);
    header->addView(chevron);

    this->addView(header);

    panel = new brls::Box(brls::Axis::COLUMN);
    panel->setClipsToBounds(true);
    panel->setHeight(0.0f);
    this->addView(panel);
}

void ProfileSwitcherView::refresh() {
    if (card)
        card->refresh();

    expanded = false;
    chevron->setText(ICON_EXPAND_MORE);
    panel->clearViews();
    panel->setHeight(0.0f);
}

float ProfileSwitcherView::measurePanel() const {
    size_t count = settings->getProfiles().size();
    float target = ROW_H;
    if (count > 0)
        target += count * ROW_H + SEP_H;
    return target;
}

void ProfileSwitcherView::setExpanded(bool value) {
    if (value == expanded && !panel->getChildren().empty())
        return;

    expanded = value;

    if (expanded) {
        chevron->setText(ICON_EXPAND_LESS);
        rebuildRows();
        float target = measurePanel();
        akira::ui::motion::animate(heightAnim, panel->getHeight(false), target, 220,
            brls::EasingFunction::quadraticOut,
            [this](float v) { panel->setHeight(v); });
    } else {
        chevron->setText(ICON_EXPAND_MORE);
        brls::Application::giveFocus(header);
        akira::ui::motion::animate(heightAnim, panel->getHeight(false), 0.0f, 200,
            brls::EasingFunction::quadraticOut,
            [this](float v) { panel->setHeight(v); },
            [this]() { panel->clearViews(); });
    }
}

void ProfileSwitcherView::rebuildRows() {
    panel->clearViews();

    const auto& profiles = settings->getProfiles();
    int64_t activeId = settings->getActiveProfileId();

    for (const auto& p : profiles)
        panel->addView(makeProfileRow(p, p.id == activeId));

    if (!profiles.empty()) {
        auto* sep = new brls::Rectangle();
        sep->setHeight(SEP_H);
        sep->setColor(akira::ui::active().surfaceLine);
        panel->addView(sep);
    }

    panel->addView(makeAddRow());
}

brls::Box* ProfileSwitcherView::makeAvatar(const std::string& label) {
    const auto& palette = akira::ui::active();

    auto* av = new brls::Box(brls::Axis::ROW);
    av->setWidth(AVATAR);
    av->setHeight(AVATAR);
    av->setCornerRadius(AVATAR / 2.0f);
    av->setBackgroundColor(palette.surfaceLine);
    av->setJustifyContent(brls::JustifyContent::CENTER);
    av->setAlignItems(brls::AlignItems::CENTER);
    av->setMarginRight(14.0f);

    std::string initial = "?";
    if (!label.empty())
        initial = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(label[0]))));

    auto* glyph = new brls::Label();
    glyph->setText(initial);
    glyph->setFontSize(18.0f);
    glyph->setTextColor(palette.textMuted);
    av->addView(glyph);

    return av;
}

brls::Box* ProfileSwitcherView::makeProfileRow(const Profile& profile, bool isActive) {
    const auto& palette = akira::ui::active();

    int64_t id = profile.id;
    std::string label = profile.label();
    std::string display = label.empty()
        ? "akira/settings/unnamed_profile"_i18n
        : settings->maskAccountName(label);

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(ROW_H);
    row->setAlignItems(brls::AlignItems::CENTER);

    auto* main = new brls::Box(brls::Axis::ROW);
    main->setGrow(1.0f);
    main->setHeight(ROW_H);
    main->setAlignItems(brls::AlignItems::CENTER);
    main->setPaddingLeft(18.0f);
    main->setPaddingRight(12.0f);
    main->setFocusable(true);
    if (isActive)
        main->setBackgroundColor(palette.surfaceElevated);
    main->registerClickAction([this, id](brls::View*) {
        selectProfile(id);
        return true;
    });

    main->addView(makeAvatar(label));

    auto* name = new brls::Label();
    name->setText(display);
    name->setFontSize(19.0f);
    name->setTextColor(isActive ? palette.text : palette.textMuted);
    name->setSingleLine(true);
    name->setGrow(1.0f);
    main->addView(name);

    if (isActive) {
        auto* check = new brls::Label();
        check->setText(ICON_CHECK);
        check->setFontSize(24.0f);
        check->setTextColor(palette.accent);
        main->addView(check);
    }

    row->addView(main);

    /*
     * Deleting and re-pairing is the only way to change a profile's mode, and offering
     * the trash only on inactive rows meant a lone profile - the common case - could
     * never be removed at all.
     */
    const bool onlyProfile = settings->getProfiles().size() == 1;

    if (!isActive || onlyProfile) {
        auto* trash = new brls::Box(brls::Axis::ROW);
        trash->setWidth(56.0f);
        trash->setHeight(ROW_H);
        trash->setJustifyContent(brls::JustifyContent::CENTER);
        trash->setAlignItems(brls::AlignItems::CENTER);
        trash->setFocusable(true);
        trash->registerClickAction([this, id, display](brls::View*) {
            confirmRemoveProfile(id, display);
            return true;
        });

        auto* trashGlyph = new brls::Label();
        trashGlyph->setText(ICON_DELETE);
        trashGlyph->setFontSize(22.0f);
        trashGlyph->setTextColor(palette.textDim);
        trash->addView(trashGlyph);

        row->addView(trash);
    }

    return row;
}

brls::Box* ProfileSwitcherView::makeAddRow() {
    const auto& palette = akira::ui::active();

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(ROW_H);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setPaddingLeft(18.0f);
    row->setPaddingRight(12.0f);
    row->setFocusable(true);
    row->registerClickAction([this](brls::View*) {
        addProfileFlow();
        return true;
    });

    auto* ring = new brls::Box(brls::Axis::ROW);
    ring->setWidth(AVATAR);
    ring->setHeight(AVATAR);
    ring->setCornerRadius(AVATAR / 2.0f);
    ring->setBorderColor(palette.accent);
    ring->setBorderThickness(1.5f);
    ring->setJustifyContent(brls::JustifyContent::CENTER);
    ring->setAlignItems(brls::AlignItems::CENTER);
    ring->setMarginRight(14.0f);

    auto* plus = new brls::Label();
    plus->setText(ICON_ADD);
    plus->setFontSize(20.0f);
    plus->setTextColor(palette.accent);
    ring->addView(plus);
    row->addView(ring);

    auto* label = new brls::Label();
    label->setText("akira/settings/add_profile"_i18n);
    label->setFontSize(19.0f);
    label->setTextColor(palette.accent);
    label->setGrow(1.0f);
    row->addView(label);

    return row;
}

void ProfileSwitcherView::selectProfile(int64_t id) {
    if (id == settings->getActiveProfileId()) {
        setExpanded(false);
        return;
    }

    settings->setActiveProfileId(id);
    settings->writeFile();
    TrophyManager::getInstance()->onActiveProfileChanged();
    HostListTab::notifyActiveProfileChanged();

    if (card)
        card->refresh();
    if (onProfileChanged)
        onProfileChanged();

    brls::Application::notify("akira/settings/profile_switched"_i18n);
    setExpanded(false);
    brls::sync([] { QrSignInView::openIfSignInExpired(); });
}

void ProfileSwitcherView::confirmRemoveProfile(int64_t id, const std::string& label) {
    const bool last = settings->getProfiles().size() == 1;
    auto* dialog = new brls::Dialog(last
        ? "akira/settings/remove_last_profile"_i18n
        : brls::getStr("akira/settings/remove_profile_named", label));
    dialog->addButton("akira/common/cancel"_i18n, [dialog]() { dialog->close(); });
    dialog->addButton("akira/settings/remove_profile"_i18n, [this, id, dialog]() {
        dialog->close();
        std::string account;
        settings->readProfile(id, [&account](const Profile& profile) { account = profile.accountId; });
        settings->removeProfile(id);
        const bool shared = std::ranges::any_of(settings->getProfiles(),
            [&account](const Profile& profile) { return profile.accountId == account; });
        if (!account.empty() && !shared) {
            if (auto* accounts = akira::ChiakiCore::current().accounts())
                accounts->forget(account);
        }
        settings->writeFile();
        TrophyManager::getInstance()->onActiveProfileChanged();
        HostListTab::notifyActiveProfileChanged();

        /*
         * Removing the last profile leaves nothing to configure, and the settings frame
         * would sit on an Account tab with no account. Onboarding owns that state.
         */
        if (settings->getProfiles().empty()) {
            brls::Application::notify("akira/settings/profile_removed"_i18n);
            /*
             * With no profile there is nothing to configure and nothing to connect to,
             * so the only coherent place to be is the home screen in its starting state.
             */
            brls::sync([]() { popToHome(); });
            return;
        }

        if (card)
            card->refresh();
        if (onProfileChanged)
            onProfileChanged();

        brls::Application::notify("akira/settings/profile_removed"_i18n);

        if (expanded) {
            rebuildRows();
            akira::ui::motion::animate(heightAnim, panel->getHeight(false), measurePanel(), 200,
                brls::EasingFunction::quadraticOut,
                [this](float v) { panel->setHeight(v); });
        }
    });
    dialog->open();
}

void ProfileSwitcherView::addProfileFlow() {
    setExpanded(false);
    brls::Application::pushActivity(new brls::Activity(new ProfileSetupView(true)));
}
