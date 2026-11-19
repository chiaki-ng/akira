#include "consoles/views/host_list_tab.hpp"
#include "app/view_services.hpp"
#include "log/log.hpp"
#if AKIRA_CLOUD
#include "cloud/views/cloud_card.hpp"
#include "cloud/cloud_connect_task.hpp"
#include "cloud/service.hpp"
#endif
#include "app/views/theme.hpp"
#include "play/views/connecting_view.hpp"
#include "play/views/host_connect_task.hpp"
#include "accounts/views/qr_sign_in_view.hpp"
#include "accounts/profile_choices.hpp"
#include "accounts/profile_switch.hpp"
#include "consoles/console_actions.hpp"
#include "consoles/debug_consoles.hpp"
#include "consoles/discovery_gate.hpp"
#include "consoles/host.hpp"
#include "consoles/remote_lookup.hpp"
#include "consoles/views/account_setup_card.hpp"
#include "consoles/views/add_host_card.hpp"
#include "consoles/views/console_card.hpp"
#include "trophies/trophy_manager.hpp"
#include "play/chiaki_core.hpp"

#include <borealis/core/i18n.hpp>

#include <string>
#include <vector>

namespace akira::consoles::views {

using namespace brls::literals;

const brls::ButtonStyle HostListTab::findRemoteStyle = {
    .shadowType              = brls::ShadowType::GENERIC,
    .hideHighlightBackground = true,
    .highlightPadding = "",
    .borderThickness  = "",
    .enabledBackgroundColor = "",
    .enabledLabelColor      = "brls/button/primary_enabled_text",
    .enabledBorderColor     = "",
    .disabledBackgroundColor = "",
    .disabledLabelColor      = "brls/button/primary_disabled_text",
    .disabledBorderColor     = "",
};

HostListTab* HostListTab::currentInstance = nullptr;
bool HostListTab::isConnecting = false;

HostListTab::HostListTab() {
    currentInstance = this;
    this->inflateFromXMLRes("xml/tabs/host_list.xml");

    services = &app::ViewServices::current();
    discovery = &app::ViewServices::current().discovery;

    discovery->setOnHostDiscovered([](Host* host) {
        const DiscoveryGate& gate = app::ViewServices::current().discoveryGate;
        const auto verdict = gate.discovered(host != nullptr, HostListTab::currentInstance != nullptr);
        if (verdict == DiscoveryGate::Discovered::NoHost) {
            log::warning("onHostDiscovered: host is null");
            return;
        }
        log::info("onHostDiscovered: isActive={}, currentInstance={}",
            gate.shown() ? "true" : "false",
            HostListTab::currentInstance ? "valid" : "null");
        switch (verdict) {
            case DiscoveryGate::Discovered::Suspended:
                log::info("onHostDiscovered: suspended, skipping {}", host->getHostName());
                return;
            case DiscoveryGate::Discovered::NoList:
                log::warning("onHostDiscovered: currentInstance is null");
                return;
            case DiscoveryGate::Discovered::NoHost:
            case DiscoveryGate::Discovered::Update:
                break;
        }
        log::info("Host discovered/updated: {}", host->getHostName());
        HostListTab::currentInstance->updateHostItem(host);
    });

    discovery->setOnHostsChanged([]() {
        if (app::ViewServices::current().discoveryGate.hostsChanged(HostListTab::currentInstance != nullptr))
            HostListTab::currentInstance->syncHostList();
    });

    if (!discovery->isServiceEnabled()) {
        discovery->setServiceEnabled(true);
    }

    initFindRemoteButton();

    profileChip = new accounts::views::ProfileChipView();
    chipSlot->addView(profileChip);
    profileChip->registerClickAction([this](brls::View*) {
        const auto& profiles = services->accounts.profiles();
        if (profiles.empty()) {
            brls::Application::notify("akira/hosts/no_profiles_add"_i18n);
            return true;
        }

#if AKIRA_CLOUD
        if (app::ViewServices::current().cloud.snapshotForActiveProfile().status.availability
                == cloud::Availability::Checking) {
            brls::Application::notify("akira/cloud/checking_wait"_i18n);
            return true;
        }
#endif

        const accounts::ProfileChoices choices(profiles, services->accounts.activeProfileId(), services->appSettings);
        std::vector<std::string> names;
        for (const auto& choice : choices.choices())
            names.push_back(choice.label.value_or("akira/settings/unnamed_profile"_i18n));

        auto* dropdown = new brls::Dropdown(
            "akira/hosts/switch_profile"_i18n, names,
            [choices](int sel) {
                const auto id = choices.idAt(sel);
                if (!id)
                    return;
                auto& services = app::ViewServices::current();
                accounts::ProfileSwitch(services.accounts, services.store,
                    [&services] { services.trophies.onActiveProfileChanged(); }).to(*id);
                brls::sync([] { accounts::views::QrSignInView::openIfSignInExpired(); });
#if AKIRA_CLOUD
                app::ViewServices::current().cloud.refreshActiveProfile(false,
                    [](const cloud::Snapshot&) { refreshSurfaces(); });
#endif
                brls::sync([]() { refreshSurfaces(); });
            },
            choices.selected());
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
    });

    applyProfileMode();

    syncHostList();
}

/*
 * Which of these surfaces exist depends on the active profile, and this tab outlives any
 * number of profile switches - so the decision cannot live in the constructor. Idempotent
 * on purpose: called once at construction and again on every switch.
 */
void HostListTab::applyProfileMode() {
    const bool legacy = services->accounts.isLegacyProfileActive();

    if (findRemoteBtn)
        findRemoteBtn->setVisibility(legacy ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

#if AKIRA_CLOUD
    /* Cloud needs an npsso: a permanent "pair to use this" card is a dead end. */
    if (legacy) {
        if (shortcutsRail) {
            railSlot->clearViews();
            shortcutsRail = nullptr;
        }
        return;
    }

    if (shortcutsRail)
        return;

    shortcutsRail = new cloud::views::CloudShortcutsRail();
    shortcutsRail->setLaunchHandler([](const cloud::Game& game) {
        const cloud::Snapshot snapshot = app::ViewServices::current().cloud.snapshotForActiveProfile();
        if (snapshot.status.plusMembership == cloud::PlusMembership::None)
        {
            auto* dialog = new brls::Dialog("akira/cloud/status_subscription_detail"_i18n);
            dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
            dialog->open();
            return;
        }
        app::ViewServices::current().discoveryGate.connectionStarted();
        play::views::startConnecting(std::make_unique<cloud::CloudConnectTask>(app::ViewServices::current().cloud,
            app::ViewServices::current().accounts, app::ViewServices::current().appSettings,
            app::ViewServices::current().dispatcher, app::ViewServices::current().strings, game));
    });
    shortcutsRail->setLeadingCard([]() -> brls::View* { return new cloud::views::CloudCard(); });
    railSlot->addView(shortcutsRail);

    app::ViewServices::current().cloud.refreshActiveProfile(false,
        [](const cloud::Snapshot&) { refreshSurfaces(); });
#endif
}

brls::View* HostListTab::getDefaultFocus() {
    if (emptyActionCard)
        return emptyActionCard->getDefaultFocus();
    return brls::Box::getDefaultFocus();
}

void HostListTab::connectToHost(Host* host) {
    if (!host || HostListTab::isConnecting)
        return;

    log::info("Connect to {}", host->getHostName());

    const auto missing = actions().missingIdentity(*host);

    if (host->isPsnRemotePlayDisabled()) {
        log::warning("Connect {}: PSN reports remote play disabled for this account",
            host->getHostName());
        brls::Application::notify("akira/connection/fail_rp_disabled"_i18n);
    }

    if (missing == PsnIdentity::Missing::AccountId) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_account_id_required"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
        return;
    }

    if (missing == PsnIdentity::Missing::OnlineId) {
        auto* dialog = new brls::Dialog("akira/hosts/psn_online_id_required"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
        return;
    }

    HostListTab::isConnecting = true;
    app::ViewServices::current().discoveryGate.connectionStarted();

    /* Local and remote alike. A local console resolves immediately, so the
     * screen is barely seen - but the activity stack ends up the same shape it
     * does for every other route, which is the point. */
    play::views::startConnecting(std::make_unique<play::views::HostConnectTask>(host));

    HostListTab::isConnecting = false;
}

void HostListTab::initFindRemoteButton() {
    /*
     * Wired unconditionally; applyProfileMode decides whether it is on screen. Find
     * Remote is a PSN device lookup, so legacy hides it rather than offering a button
     * that always fails.
     */
    findRemoteBtn->setStyle(&findRemoteStyle);

    findRemoteGate.attach(
        findRemoteBtn,
        app::views::active().accent,
        "akira/hosts/find_remote"_i18n,
        "akira/hosts/find_remote_busy"_i18n,
        "akira/hosts/find_remote_wait",
        []() { return app::ViewServices::current().discovery.getRemoteRefreshStatus(); }
    );

    findRemoteBtn->registerClickAction([this](brls::View* view) {
        if (!findRemoteGate.isReady()) {
            return true;
        }

        auto* accounts = play::ChiakiCore::current().accounts();
        const RemoteLookup lookup(accounts ? std::optional(accounts->state()) : std::nullopt);
        if (!lookup.allowed()) {
            brls::Application::notify("akira/hosts/no_psn_token"_i18n);
            return true;
        }

        auto onComplete = [](const DiscoveryManager::RemoteRefreshResult& result) {
            if (!result.success) {
                brls::Application::notify(brls::getStr("akira/hosts/token_refresh_failed", result.message));
            }

            if (!HostListTab::currentInstance) {
                return;
            }
            HostListTab::currentInstance->syncHostList();
            if (HostListTab::currentInstance->findRemoteBtn) {
                brls::Application::giveFocus(HostListTab::currentInstance->findRemoteBtn);
            }
        };

        if (lookup.notice() == RemoteLookup::Notice::Finding) {
            brls::Application::notify("akira/hosts/finding_remote"_i18n);
        } else {
            brls::Application::notify("akira/hosts/token_expired_refreshing"_i18n);
        }

        discovery->refreshRemoteDevices(onComplete, true);
        findRemoteGate.apply();

        return true;
    });
}

HostListTab::~HostListTab() {
    if (currentInstance == this) {
        currentInstance = nullptr;
    }

    hostItems.clear();

    discovery->setOnHostDiscovered(nullptr);
    discovery->setOnHostsChanged(nullptr);
}

void HostListTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    services->discoveryGate.appeared();
    log::debug("HostListTab::willAppear - resuming discovery callbacks");
    findRemoteGate.start();
    if (profileChip) profileChip->refresh();
#if AKIRA_CLOUD
    if (shortcutsRail) shortcutsRail->refresh();
    app::ViewServices::current().cloud.refreshActiveProfile(false,
        [](const cloud::Snapshot&) { refreshSurfaces(); });
#endif
    syncHostList();
}

void HostListTab::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    services->discoveryGate.disappeared();
    log::debug("HostListTab::willDisappear - pausing discovery callbacks");
    findRemoteGate.stop();
}

brls::View* HostListTab::create() {
    return new HostListTab();
}

ConsoleActions HostListTab::actions() {
    auto& services = app::ViewServices::current();
    return ConsoleActions(services.hosts, services.store, services.accounts, services.developer, services.wireGuard,
                          play::ChiakiCore::current());
}

ConsoleList HostListTab::consoleList() {
    auto& services = app::ViewServices::current();
    return ConsoleList(services.hosts.facts(),
        ConsoleList::profileFor(!services.accounts.profiles().empty(), services.accounts.isLegacyProfileActive()));
}

void HostListTab::refreshRailsIfActive() {
    app::ViewServices::current().discoveryGate.connectionEnded();
    if (!currentInstance)
        return;
    brls::sync([]() {
        if (!currentInstance)
            return;
#if AKIRA_CLOUD
        if (currentInstance->shortcutsRail)
            currentInstance->shortcutsRail->refresh();
#endif

        brls::View* f = brls::Application::getCurrentFocus();
        bool underHome = false;
        for (brls::View* v = f; v; v = v->getParent())
            if (v == currentInstance) { underHome = true; break; }
        log::info("refreshRailsIfActive: focus={} isActive={} underHome={} stack={}",
            f ? f->describe() : std::string("null"),
            currentInstance->services->discoveryGate.shown() ? "yes" : "no",
            underHome ? "yes" : "no",
            (int)brls::Application::getActivitiesStack().size());
        /*
         * Only while this really is the screen in front.
         *
         * isActive says the home tab is the selected tab, not that its activity
         * is on top - and this runs from onResume through brls::sync, a frame
         * later. The remote route pops the connection view and pushes the
         * stream view in the same breath, so the pop fires onResume, and by the
         * time the callback runs the stream view and the controller picker are
         * both above us. The old guard then read a focus it did not recognise
         * as its own and took it back, leaving the picker on screen with focus
         * four activities beneath it - where L and R are the home tab's, and
         * open trophies out from under the stream.
         */
        const auto stack = brls::Application::getActivitiesStack();
        const bool onTop = !stack.empty() &&
                           currentInstance->getParentActivity() == stack.back();

        if (currentInstance->services->discoveryGate.shown() && !underHome && onTop)
            brls::Application::giveFocus(currentInstance);
    });
}

void HostListTab::notifyActiveProfileChanged() {
#if AKIRA_CLOUD
    app::ViewServices::current().cloud.refreshActiveProfile(false);
#endif
    if (!currentInstance)
        return;
    currentInstance->applyProfileMode();
    currentInstance->syncHostList();
    if (currentInstance->profileChip)
        currentInstance->profileChip->refresh();
#if AKIRA_CLOUD
    if (currentInstance->shortcutsRail)
        currentInstance->shortcutsRail->refresh();
#endif
}

void HostListTab::notifyAccountNameDisplayChanged() {
    if (!currentInstance || !currentInstance->profileChip)
        return;
    currentInstance->profileChip->refresh();
}

void HostListTab::syncHostList() {
    log::debug("Syncing host list...");

    if (!hostContainer) {
        log::error("syncHostList: hostContainer is null");
        return;
    }
    brls::View* focused = brls::Application::getCurrentFocus();
    bool childFocused = false;
    for (brls::View* v = focused; v; v = v->getParent()) {
        if (v == this) {
            childFocused = true;
            break;
        }
    }

    // borealis really hates it when you remove a view.
    // just rebuild the damn thing.
    if (childFocused)
        brls::Application::giveFocus(this);

    hostContainer->clearViews();
    hostItems.clear();

    /*
     * activeRegistration() keys off the active profile, so with no profile every host
     * reports hasRpKey() == false and none of them can be connected to or registered.
     * Listing a console the user cannot use is worse than not listing it: the whole
     * screen becomes the same starting state a fresh install shows.
     */
    const ConsoleList consoles = consoleList();
    const bool noProfile = !consoles.hasProfile();
    if (noProfile)
        entrancePlayed = false;

    bool animateEntrance = !entrancePlayed;
    int shownIndex = 0;

    const int perRow = 6;
    int col = 0;
    brls::Box* row = nullptr;
    for (const ConsoleEntry& entry : consoles.shown()) {
        Host* host = services->hosts.entries().at(entry.facts().catalogName).get();
        if (col == 0) {
            row = new brls::Box();
            row->setAxis(brls::Axis::ROW);
            hostContainer->addView(row);
        }
        auto* item = new ConsoleCard(host);
        hostItems[host] = item;
        row->addView(item);
        if (animateEntrance)
            item->playEntrance(shownIndex * 45);
        shownIndex++;
        col = (col + 1) % perRow;
    }

    bool hasHosts = !hostItems.empty();

    if (services->developer.fakeHosts()) {
        static std::vector<Host*> fakeHosts;
        if (fakeHosts.empty()) {
            for (const DebugConsoles::Console& console : DebugConsoles::list()) {
                Host* h = new Host(HostRecord{.nickname = console.nickname}, services->accounts, services->appSettings,
                                   services->dispatcher);
                h->setFirmware(Host::Firmware::newest(libchiaking::consoles::Generation::ps5));
                h->setPresence(console.resting ? Host::Presence::rest_mode : Host::Presence::ready);
                h->setHostType(console.type);
                fakeHosts.push_back(h);
            }
        }
        for (Host* h : fakeHosts) {
            if (col == 0) {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                hostContainer->addView(row);
            }
            row->addView(new ConsoleCard(h));
            col = (col + 1) % perRow;
        }
        hasHosts = true;
    }

    if (hasHosts && animateEntrance)
        entrancePlayed = true;

    bool hasProfile = !noProfile;

    emptyActionCard = nullptr;

    if (hasHosts) {
        if (hasProfile) {
            if (col == 0 || !row) {
                row = new brls::Box();
                row->setAxis(brls::Axis::ROW);
                hostContainer->addView(row);
            }
            row->addView(new AddHostCard());
#if AKIRA_CLOUD
            for (auto* child : row->getChildren())
                child->setCustomNavigationRoute(brls::FocusDirection::DOWN, "cloud/rail/lead");
#endif
        }
    } else {
        auto* cardRow = new brls::Box();
        cardRow->setAxis(brls::Axis::ROW);
        cardRow->setWidthPercentage(100.0f);
        cardRow->setJustifyContent(brls::JustifyContent::CENTER);
        hostContainer->addView(cardRow);
        if (hasProfile) {
            auto* addHost = new AddHostCard();
            cardRow->addView(addHost);
            emptyActionCard = addHost;
        } else {
            auto* card = new AccountSetupCard();
            cardRow->addView(card);
            emptyActionCard = card;
        }
    }

    if (emptyMessage) {
        emptyMessage->setVisibility((!hasHosts && hasProfile) ? brls::Visibility::VISIBLE
                                                              : brls::Visibility::GONE);
    }

    if (childFocused || noProfile) {
        if (hasHosts) {
            brls::Application::giveFocus(hostContainer);
        } else if (emptyActionCard) {
            brls::Application::giveFocus(emptyActionCard);
        } else if (findRemoteBtn) {
            brls::Application::giveFocus(findRemoteBtn);
        }
    }

    log::debug("Host list sync complete, {} hosts", hostItems.size());
}

void HostListTab::updateHostItem(Host* host) {
    if (!host) return;

    auto it = hostItems.find(host);
    if (it != hostItems.end()) {
        it->second->updateState();
        log::debug("Updated host item: {}", host->getHostName());
    } else {
        log::debug("Host not found in items, syncing: {}", host->getHostName());
        syncHostList();
    }
}

void HostListTab::refreshSurfaces() {
    if (!currentInstance)
        return;
    currentInstance->syncHostList();
    if (currentInstance->profileChip)
        currentInstance->profileChip->refresh();
#if AKIRA_CLOUD
    if (currentInstance->shortcutsRail)
        currentInstance->shortcutsRail->refresh();
#endif
}

void HostListTab::consoleChanged(Host* host) {
    if (currentInstance)
        currentInstance->updateHostItem(host);
}

void HostListTab::consoleLinked() {
    if (currentInstance) {
        currentInstance->syncHostList();
        if (currentInstance->findRemoteBtn) {
            brls::Application::giveFocus(currentInstance->findRemoteBtn);
        }
    }
}

void HostListTab::consoleRemoved() {
    if (currentInstance) {
        // If this was the last host, give focus to Find Remote button first
        // to avoid dangling focus pointer when host list becomes empty
        if (currentInstance->hostItems.size() <= 1 &&
            currentInstance->findRemoteBtn) {
            brls::Application::giveFocus(currentInstance->findRemoteBtn);
        }
        currentInstance->syncHostList();
    }
}

}
