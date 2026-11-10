#include "views/pair_view.hpp"

#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "core/chiaki_core.hpp"
#include "core/settings_manager.hpp"
#include "core/trophy_manager.hpp"
#include "ui/theme.hpp"
#include "views/host_list_tab.hpp"

#include <borealis/core/i18n.hpp>

#include <cstdio>
#include <string>

using namespace brls::literals;
using namespace akira::pair;

namespace {

std::string spaced(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); i++) {
        if (i > 0)
            out += "  ";
        out += s[i];
    }
    return out;
}

const char* statusKey(ListenerEvent event) {
    switch (event) {
        case ListenerEvent::Listening:
            return "akira/pair/status_waiting";
        case ListenerEvent::ClientConnected:
            return "akira/pair/status_connecting";
        case ListenerEvent::Imported:
            return "akira/pair/status_imported";
        case ListenerEvent::BadCode:
            return "akira/pair/status_bad_code";
        case ListenerEvent::LockedOut:
            return "akira/pair/status_locked";
        case ListenerEvent::TimedOut:
            return "akira/pair/status_timed_out";
        case ListenerEvent::Error:
        default:
            return "akira/pair/status_error";
    }
}

}

PairView::PairView(bool createProfile) : createProfile(createProfile) {
    this->inflateFromXMLRes("xml/views/pair_view.xml");

    codeLabel = dynamic_cast<brls::Label*>(this->getView("pair/code"));
    addrIpLabel = dynamic_cast<brls::Label*>(this->getView("pair/addrIp"));
    addrPortLabel = dynamic_cast<brls::Label*>(this->getView("pair/addrPort"));
    timerLabel = dynamic_cast<brls::Label*>(this->getView("pair/timer"));
    statusLabel = dynamic_cast<brls::Label*>(this->getView("pair/status"));
    closeBtn = dynamic_cast<brls::Button*>(this->getView("pair/close"));

    if (closeBtn) {
        closeBtn->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
    }

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });

    setFocusable(true);

    listenPort = SettingsManager::getInstance()->getCompanionPort();
    const auto& palette = akira::ui::active();
    std::string ip = PairListener::localIpv4();
    if (addrIpLabel) {
        addrIpLabel->setText(ip.empty() ? "-" : ip);
        addrIpLabel->setTextColor(palette.accent);
    }
    if (addrPortLabel) {
        addrPortLabel->setText(brls::getStr("akira/pair/port_fmt", listenPort));
        addrPortLabel->setTextColor(palette.warning);
    }

    advertiser.start(listenPort);
    startListening();
}

PairView::~PairView() {
    *alive = false;
    advertiser.stop();
    listener.stop();
}

brls::View* PairView::create() {
    return new PairView();
}

void PairView::startListening() {
    std::string code = PairListener::generateCode();
    if (codeLabel)
        codeLabel->setText(spaced(code));

    windowStart = std::chrono::steady_clock::now();
    lastShownSecond = -1;
    counting = true;

    auto guard = alive;
    listener.start(
        listenPort, code,
        [this, guard](ListenerEvent event) {
            brls::sync([this, guard, event]() {
                if (!*guard)
                    return;
                this->onEvent(event);
            });
        },
        [guard, create = createProfile](const PairedCredentials& creds) {
            PairedCredentials copy = creds;
            brls::sync([guard, copy, create]() {
                if (!*guard)
                    return;
                PairView::applyCredentials(copy, create);
            });
        });
}

void PairView::draw(NVGcontext* vg, float x, float y, float width, float height,
                    brls::Style style, brls::FrameContext* ctx) {
    if (counting && timerLabel) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - windowStart)
                           .count();
        int remaining = PairListener::kWindowSeconds - static_cast<int>(elapsed);
        if (remaining < 0)
            remaining = 0;
        if (remaining != lastShownSecond) {
            lastShownSecond = remaining;
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d:%02d", remaining / 60, remaining % 60);
            timerLabel->setText(brls::getStr("akira/pair/timer_fmt", std::string(buf)));
        }
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

void PairView::onEvent(ListenerEvent event) {
    if (event == ListenerEvent::TimedOut) {
        startListening();
        return;
    }

    if (event == ListenerEvent::Imported) {
        brls::Application::popActivity();
        return;
    }

    if (event == ListenerEvent::LockedOut) {
        counting = false;
        if (timerLabel)
            timerLabel->setText("");
    }

    if (statusLabel)
        statusLabel->setText(brls::getStr(statusKey(event)));
}

void PairView::applyCredentials(const PairedCredentials& creds, bool createProfile) {
    SettingsManager* settings = SettingsManager::getInstance();

    brls::Logger::info("Pair push: create={} active={} onlineId={} accountId={} npsso={} refresh={} mobile={}",
        createProfile, settings->getActiveProfileId(), !creds.onlineId.empty(),
        !creds.accountId.empty(), !creds.npsso.empty(), !creds.refreshToken.empty(),
        creds.hasMobile);
    settings->logProfileCensus("pair-before");

    if (createProfile || !settings->getActiveProfile()) {
        Profile fresh;
        int64_t id = settings->addProfile(fresh);
        settings->setActiveProfileId(id);
    }

    Profile* profile = settings->getActiveProfile();

    /*
     * Switching an existing profile between modes is not supported: delete it and add a
     * new one. Halfway states - legacy with tokens, or a token-less profile still marked
     * normal - are the ones that would silently leak requests or silently block them.
     */
    if (profile && profile->legacy) {
        brls::Logger::warning("Rejecting pair: profile {} is legacy", profile->id);
        brls::Application::notify("akira/pair/mode_mismatch"_i18n);
        return;
    }

    /*
     * A legacy account id is typed by hand, so it can be well-formed and still belong to
     * someone else. Registrations already on this profile were minted against the old
     * one, and their rp keys are bound to that account - the console rejects the session
     * later with nothing that names the cause.
     */
    if (profile && !creds.accountId.empty() && !profile->accountId.empty()
        && profile->accountId != creds.accountId) {
        brls::Logger::warning("Pair changes the account id on profile {}", profile->id);
        brls::Application::notify("akira/pair/account_changed"_i18n);
    }

    if (!creds.onlineId.empty())
        settings->setPsnOnlineId(nullptr, creds.onlineId);
    if (!creds.accountId.empty())
        settings->setPsnAccountId(nullptr, creds.accountId);

    settings->refreshLegacyGate();
    settings->writeFile();
    settings->logProfileCensus("pair-after");

    auto* accounts = akira::ChiakiCore::current().accounts();
    if (!accounts || creds.npsso.empty()) {
        brls::Logger::warning("Pair push carried no usable NPSSO, nothing to sign in with");
        brls::Application::notify(brls::getStr("akira/hosts/token_refresh_failed", "NPSSO"));
        return;
    }

    const int64_t profileId = settings->getActiveProfileId();
    accounts->signIn(profileId, creds.npsso, creds.duid,
        [profileId](psn::SignInOutcome outcome, const psn::Accounts::User& user) {
            if (outcome != psn::SignInOutcome::SignedIn || !user) {
                brls::Logger::warning("Pair push sign-in for profile {} ended without a user", profileId);
                if (outcome != psn::SignInOutcome::Superseded)
                    brls::Application::notify(brls::getStr("akira/hosts/token_refresh_failed", "PSN"));
                return;
            }
            adoptSignedInUser(profileId, user);
            brls::Logger::info("Signed in to PSN from the pairing push");
        });
}

void PairView::adoptSignedInUser(int64_t profileId, const psn::Accounts::User& user) {
    SettingsManager* settings = SettingsManager::getInstance();
    settings->updateProfile(profileId, [&user](Profile& profile) {
        profile.accountId = psn::Accounts::base64Of(user->account_id());
        profile.onlineId = user->online_id();
    });
    settings->refreshLegacyGate();
    settings->writeFile();

    TrophyManager::getInstance()->onActiveProfileChanged();
    /*
     * A new token can mean a different account behind the same profile, and the
     * cached catalog carries the previous one's owned games.
     */
#if AKIRA_CLOUD
    cloud::Service::instance().clearCatalogCache();
    cloud::Service::instance().markActiveProfileDirty();
    cloud::Service::instance().refreshActiveProfile(true);
#endif
    HostListTab::notifyActiveProfileChanged();
}
