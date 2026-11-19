#include "consoles/views/console_card.hpp"
#include "app/view_services.hpp"
#include "log/log.hpp"

#include <borealis/core/i18n.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "accounts/accounts.hpp"
#include "app/views/motion.hpp"
#include "app/views/theme.hpp"
#include "consoles/console_actions.hpp"
#include "consoles/host.hpp"
#include "consoles/register_flow.hpp"
#include "consoles/views/auto_registration_screen.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "consoles/views/pin_registration_screen.hpp"
#include "play/chiaki_core.hpp"

namespace akira::consoles::views {

using namespace brls::literals;

ConsoleCard::ConsoleCard(Host* host)
    : host(host)
    , hostName(host->getHostName())
{
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::FLEX_START);
    this->setWidth(174);
    this->setHeight(238);
    this->setPadding(14);
    this->setMarginRight(14);
    this->setMarginBottom(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(14);
    this->setFocusable(true);

    consoleImg = new brls::Image();
    consoleImg->setImageFromRes(host->isPS5() ? "img/console/ps5.png" : "img/console/ps4.png");
    consoleImg->setScalingType(brls::ImageScalingType::FIT);
    consoleImg->setDimensions(146, 118);
    consoleImg->setMarginBottom(12);
    this->addView(consoleImg);

    nameLabel = new brls::Label();
    nameLabel->setFontSize(22);
    nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    nameLabel->setMarginBottom(6);
    nameLabel->setAutoAnimate(true);
    this->addView(nameLabel);

    typePill = new brls::Box();
    typePill->setAxis(brls::Axis::ROW);
    typePill->setCornerRadius(11);
    typePill->setPaddingLeft(11);
    typePill->setPaddingRight(11);
    typePill->setPaddingTop(2);
    typePill->setPaddingBottom(3);
    typePill->setMarginBottom(8);

    typeLabel = new brls::Label();
    typeLabel->setFontSize(16);
    typePill->addView(typeLabel);
    this->addView(typePill);

    statusPill = new brls::Box();
    statusPill->setAxis(brls::Axis::ROW);
    statusPill->setCornerRadius(11);
    statusPill->setPaddingLeft(11);
    statusPill->setPaddingRight(11);
    statusPill->setPaddingTop(2);
    statusPill->setPaddingBottom(3);

    statusLabel = new brls::Label();
    statusLabel->setFontSize(16);
    statusPill->addView(statusLabel);
    this->addView(statusPill);

    this->registerClickAction([this](brls::View*) {
        doPrimaryAction();
        return true;
    });
    this->registerAction("akira/hosts/options"_i18n, brls::ControllerButton::BUTTON_Y,
        [this](brls::View*) { openContextMenu(); return true; }, false);

    app::views::motion::liftOnFocus(this, focusAnim);

    updateState();
}

void ConsoleCard::playEntrance(int delayMs)
{
    app::views::motion::fadeIn(this, entranceAnim, delayMs, 300);
}

void ConsoleCard::updateState()
{
    const ConsoleEntry entry = HostListTab::consoleList().entry(host->facts());
    nameLabel->setText(entry.displayName());

    std::string typeText;
    NVGcolor typeColor;
    switch (entry.badge()) {
        case ConsoleEntry::Badge::Remote:
            typeText = "akira/hosts/remote"_i18n;
            typeColor = app::views::active().accentStrong;
            break;
        case ConsoleEntry::Badge::Manual:
            typeText = "akira/hosts/manual"_i18n;
            typeColor = app::views::active().media;
            break;
        case ConsoleEntry::Badge::Auto:
            typeText = "akira/hosts/auto"_i18n;
            typeColor = app::views::active().accent;
            break;
        case ConsoleEntry::Badge::Local:
            typeText = "akira/hosts/local"_i18n;
            typeColor = app::views::active().textMuted;
            break;
    }
    typeLabel->setText(typeText);
    typeLabel->setTextColor(typeColor);
    typePill->setBackgroundColor(app::views::withAlpha(typeColor, 0x2e));

    std::string pillText;
    NVGcolor fg = app::views::active().textMuted;
    switch (entry.status()) {
        case ConsoleEntry::Status::Link:
            pillText = "akira/hosts/link"_i18n;
            fg = app::views::active().accent;
            break;
        case ConsoleEntry::Status::Ready:
            pillText = "akira/hosts/ready"_i18n;
            fg = app::views::active().success;
            break;
        case ConsoleEntry::Status::Register:
            pillText = "akira/hosts/register"_i18n;
            fg = app::views::active().accent;
            break;
        case ConsoleEntry::Status::Standby:
            pillText = "akira/hosts/standby"_i18n;
            break;
        case ConsoleEntry::Status::Presence:
            pillText = entry.facts().presence;
            break;
    }
    const bool dim = entry.dimmed();
    statusLabel->setText(pillText);
    statusLabel->setTextColor(fg);
    statusPill->setBackgroundColor(app::views::withAlpha(fg, 0x2e));

    int wantDim = dim ? 1 : 0;
    if (wantDim != lastDim) {
        float target = dim ? 0.5f : 1.0f;
        if (lastDim < 0) {
            consoleImg->setAlpha(target);
            dimAnim.reset(target);
        } else {
            app::views::motion::animate(dimAnim, dimAnim.getValue(), target, 260,
                brls::EasingFunction::quadraticOut,
                [this](float v) { consoleImg->setAlpha(v); });
        }
        lastDim = wantDim;
    }
}

void ConsoleCard::doPrimaryAction()
{
    const auto action = HostListTab::consoleList().entry(host->facts()).primaryAction();
    if (!action) {
        log::warning("No primary action for {} (remote={}, needsLink={}, discovered={}, rpkey={}, canAuto={})",
            host->getHostName(), host->isRemote(), host->needsLink(), host->isDiscovered(), host->hasRpKey(), host->canPairOverInternet());
        return;
    }
    switch (*action) {
        case ConsoleEntry::Action::Link: doLink(); break;
        case ConsoleEntry::Action::Connect: doConnect(); break;
        case ConsoleEntry::Action::Register: doRegister(); break;
        case ConsoleEntry::Action::Wake: doWake(); break;
    }
}

void ConsoleCard::openContextMenu()
{
    std::vector<std::string> options;
    std::vector<std::function<void()>> handlers;
    for (const ConsoleEntry::Option option : HostListTab::consoleList().entry(host->facts()).options()) {
        switch (option) {
            case ConsoleEntry::Option::ConsolePin:
                options.push_back("akira/host_settings/console_pin"_i18n);
                handlers.emplace_back([this] { doConsolePIN(); });
                break;
            case ConsoleEntry::Option::AddAddress:
                options.push_back("akira/hosts/add_address"_i18n);
                handlers.emplace_back([this] { doAddress(); });
                break;
            case ConsoleEntry::Option::SetAddress:
                options.push_back("akira/hosts/set_address"_i18n);
                handlers.emplace_back([this] { doAddress(); });
                break;
            case ConsoleEntry::Option::TakionVersion:
                options.push_back("akira/hosts/takion_version"_i18n);
                handlers.emplace_back([this] { doTakionPin(); });
                break;
            case ConsoleEntry::Option::Delete:
                options.push_back("akira/hosts/delete"_i18n);
                handlers.emplace_back([this] { doDelete(); });
                break;
        }
    }
    if (options.empty()) return;
    auto* dropdown = new brls::Dropdown("akira/hosts/options"_i18n, options,
        [](int) {}, 0,
        [handlers](int sel) {
            if (sel < 0 || sel >= static_cast<int>(handlers.size())) return;
            handlers[static_cast<size_t>(sel)]();
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void ConsoleCard::doConnect()
{
    HostListTab::connectToHost(host);
}

void ConsoleCard::doWake()
{
    if (HostListTab::actions().wake(*host)) {
        brls::Application::notify(brls::getStr("akira/hosts/wake_sent", host->getHostName()));
    } else {
        brls::Application::notify(brls::getStr("akira/hosts/wake_failed", host->getHostName()));
    }
}

void ConsoleCard::doRegister()
{
    if (app::ViewServices::current().registration.registering()) {
        return;
    }

    log::info("Register button clicked for {}", host->getHostName());

    auto* services = &app::ViewServices::current();
    auto* accounts = app::ViewServices::current().core.accounts();
    log::info("Register {}: remoteDuid='{}' signedIn={} hasAccountId={} canAuto={}",
        host->getHostName(), host->getRemoteDuid(),
        accounts && accounts->state() == accounts::SessionState::Valid,
        !services->accounts.psnAccountId().empty(), host->canPairOverInternet());
    const auto missing = HostListTab::actions().missingIdentity(*host);

    if (missing == PsnIdentity::Missing::AccountId) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_account_id_required_register"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() {
            dialog->close();
        });
        dialog->open();
        return;
    }

    if (missing == PsnIdentity::Missing::OnlineId) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_online_id_required_register"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() {
            dialog->close();
        });
        dialog->open();
        return;
    }

    if (host->canPairOverInternet()) {
        std::vector<std::string> options = {
            "akira/hosts/register_auto"_i18n,
            "akira/hosts/register_pin"_i18n
        };
        Host* hp = host;
        auto* dropdown = new brls::Dropdown("akira/hosts/register_how"_i18n, options,
            [](int) {}, 0,
            [hp](int sel) {
                if (sel == 0) {
                    AutoRegistrationScreen::start(hp);
                } else if (sel == 1) {
                    PinRegistrationScreen::open(hp);
                }
            });
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return;
    }

    PinRegistrationScreen::open(host);
}

void ConsoleCard::doLink()
{
    log::info("Link button clicked for {}", host->getHostName());

    const std::vector<std::string> hostNames = HostListTab::consoleList().linkTargets();

    if (hostNames.empty()) {
        brls::Application::notify("akira/hosts/no_registered_hosts"_i18n);
        return;
    }

    Host* remoteHost = host;
    auto* dropdown = new brls::Dropdown(
        "akira/hosts/select_host_to_link"_i18n,
        hostNames,
        [remoteHost, hostNames](int selected) {
            if (selected < 0 || selected >= (int)hostNames.size()) return;

            const auto remoteName = HostListTab::actions().link(*remoteHost, hostNames[selected]);
            if (!remoteName) {
                if (remoteName.error() == ConsoleActions::LinkRefusal::NameTaken)
                    brls::Application::notify(brls::getStr("akira/hosts/link_name_taken",
                        ConsoleEntry(remoteHost->facts(), false).displayName()));
                return;
            }

            brls::Application::notify(brls::getStr("akira/hosts/linked_to", *remoteName));

            brls::sync([]() { HostListTab::consoleLinked(); });
        }
    );
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void ConsoleCard::doConsolePIN()
{
    std::string current = host->getConsolePIN();
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (!HostListTab::actions().setConsolePin(*host, std::move(text))) {
                brls::Application::notify("akira/host_settings/console_pin_invalid"_i18n);
                return;
            }
            brls::Application::notify("akira/host_settings/console_pin_saved"_i18n);
        },
        "akira/host_settings/console_pin"_i18n, "akira/host_settings/console_pin_hint"_i18n, 4, current, 0);
}

void ConsoleCard::doAddress()
{
    Host* target = host;
    const std::string current = host->getHostAddr();
    brls::Application::getImeManager()->openForText(
        [target](std::string text) {
            const auto moved = HostListTab::actions().moveTo(*target, text);
            if (!moved) {
                brls::Application::notify(moved.error() == Host::AddressRefusal::Invalid
                        ? "akira/hosts/address_invalid"_i18n
                        : "akira/hosts/address_not_saved"_i18n);
                return;
            }
            if (*moved == ConsoleActions::AddressChange::Unchanged) return;
            brls::Application::notify(brls::getStr("akira/hosts/address_saved", target->getHostName(), text));
            HostListTab::consoleChanged(target);
        },
        current.empty() ? "akira/hosts/add_address"_i18n : "akira/hosts/set_address"_i18n,
        "akira/hosts/address_hint"_i18n, 255, current, 0);
}

void ConsoleCard::doTakionPin()
{
    const TakionPinChoice choice = HostListTab::actions().takionPins(*host);
    const auto configured = choice.configuredVersion();
    const std::string fallback = configured
        ? "v" + std::to_string(*configured)
        : "akira/settings/dev_takion_version_auto"_i18n;
    std::vector<std::string> labels{brls::getStr("akira/hosts/takion_follow_settings", fallback)};
    std::ranges::transform(choice.versions(), std::back_inserter(labels),
        [](std::uint32_t version) { return "v" + std::to_string(version); });
    Host* target = host;
    auto* dropdown = new brls::Dropdown("akira/hosts/takion_version"_i18n, labels,
        [](int) {}, choice.selected(),
        [target, choice](int sel) {
            if (!choice.contains(sel)) return;
            const auto version = choice.versionAt(sel);
            HostListTab::actions().pinTakion(*target, version);
            if (!version) {
                brls::Application::notify(brls::getStr("akira/hosts/takion_follows", target->getHostName()));
            } else {
                brls::Application::notify(
                    brls::getStr("akira/hosts/takion_pinned", target->getHostName(), std::to_string(*version)));
            }
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void ConsoleCard::doDelete()
{
    std::string hostName = host->getHostName();
    log::info("Delete button clicked for {}", hostName);

    auto* dialog = new brls::Dialog(brls::getStr("akira/hosts/delete_confirm", hostName));
    dialog->addButton("akira/common/cancel"_i18n, []() {});
    dialog->addButton("akira/common/delete"_i18n, [hostName]() {
        HostListTab::actions().remove(hostName);
        brls::Application::notify("akira/hosts/host_deleted"_i18n);
        HostListTab::consoleRemoved();
    });
    dialog->open();
}

}
