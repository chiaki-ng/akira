#include "views/qr_sign_in_view.hpp"

#include "core/chiaki_core.hpp"
#include "core/settings_manager.hpp"
#include "ui/qr_code_view.hpp"
#include "ui/theme.hpp"
#include "views/pair_view.hpp"

#include <borealis/core/i18n.hpp>

#include <format>

using namespace brls::literals;
using libchiaking::users::QrSignInState;
using libchiaking::users::QrSignInStep;
using libchiaking::users::SignInErrorKind;

QrSignInView::QrSignInView(bool createProfile) : createProfile(createProfile) {
    this->inflateFromXMLRes("xml/views/qr_sign_in.xml");

    const auto& palette = akira::ui::active();

    if (auto* card = dynamic_cast<brls::Box*>(this->getView("qr_sign_in/card"))) {
        card->setBackgroundColor(palette.surface);
        card->setBorderColor(palette.surfaceLine);
        card->setBorderThickness(1.0f);
    }
    tile = dynamic_cast<brls::Box*>(this->getView("qr_sign_in/tile"));
    placeholder = dynamic_cast<brls::Label*>(this->getView("qr_sign_in/placeholder"));
    timerLabel = dynamic_cast<brls::Label*>(this->getView("qr_sign_in/timer"));
    statusLabel = dynamic_cast<brls::Label*>(this->getView("qr_sign_in/status"));
    retryButton = dynamic_cast<brls::Button*>(this->getView("qr_sign_in/retry"));
    closeButton = dynamic_cast<brls::Button*>(this->getView("qr_sign_in/close"));

    if (retryButton) {
        retryButton->registerClickAction([this](brls::View*) {
            start();
            return true;
        });
    }
    if (closeButton) {
        closeButton->registerClickAction([](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
    }

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });

    start();
}

QrSignInView::~QrSignInView() {
    if (auto* accounts = akira::ChiakiCore::current().accounts())
        accounts->cancelSignIn();
}

void QrSignInView::openIfSignInExpired() {
    auto* accounts = akira::ChiakiCore::current().accounts();
    if (!accounts || accounts->state() != psn::SessionState::Expired)
        return;
    brls::Logger::info("PSN sign-in for the active profile has expired; asking for a new one");
    brls::Application::pushActivity(new brls::Activity(new QrSignInView()));
}

brls::View* QrSignInView::getDefaultFocus() {
    if (retryButton && retryButton->getVisibility() == brls::Visibility::VISIBLE)
        return retryButton;
    return closeButton ? closeButton : brls::Box::getDefaultFocus();
}

void QrSignInView::start() {
    failureShown = false;
    clearCode();
    if (retryButton) {
        const bool focused = brls::Application::getCurrentFocus() == retryButton;
        retryButton->setVisibility(brls::Visibility::GONE);
        if (focused && closeButton)
            brls::Application::giveFocus(closeButton);
    }

    auto* accounts = akira::ChiakiCore::current().accounts();
    if (!accounts) {
        showFailure("akira/qr_sign_in/status_failed"_i18n);
        return;
    }
    showStatus("akira/qr_sign_in/status_preparing"_i18n, akira::ui::active().textMuted);
    accounts->signInWithQr(
        [this](const QrSignInState& state) { onProgress(state); },
        [this](psn::SignInOutcome outcome, const psn::Accounts::User& user) { onSignedIn(outcome, user); });
}

void QrSignInView::onProgress(const QrSignInState& state) {
    const auto& palette = akira::ui::active();
    switch (state.step()) {
        case QrSignInStep::preparing:
            clearCode();
            showStatus("akira/qr_sign_in/status_preparing"_i18n, palette.textMuted);
            break;
        case QrSignInStep::awaiting_approval:
            showCode(state.approval_url());
            if (const auto expires = state.expires_at())
                deadline = std::chrono::steady_clock::now() +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        *expires - std::chrono::system_clock::now());
            lastShownSecond = -1;
            showStatus("akira/qr_sign_in/status_waiting"_i18n, palette.textMuted);
            break;
        case QrSignInStep::signing_in:
            deadline.reset();
            if (timerLabel)
                timerLabel->setText("");
            showStatus("akira/qr_sign_in/status_signing_in"_i18n, palette.accent);
            break;
        case QrSignInStep::signed_in:
            break;
        case QrSignInStep::failed:
            clearCode();
            showFailure(brls::getStr(failureKey(state.failure().value_or(SignInErrorKind::unavailable))));
            break;
    }
}

void QrSignInView::onSignedIn(psn::SignInOutcome outcome, const psn::Accounts::User& user) {
    if (outcome != psn::SignInOutcome::SignedIn || !user) {
        brls::Logger::warning("QR sign-in ended without a user");
        if (!failureShown && outcome != psn::SignInOutcome::Superseded)
            showFailure("akira/qr_sign_in/status_failed"_i18n);
        return;
    }

    SettingsManager* settings = SettingsManager::getInstance();
    if (createProfile || !settings->getActiveProfile())
        settings->setActiveProfileId(settings->addProfile(Profile{}));

    const Profile* profile = settings->getActiveProfile();
    if (!profile)
        return;
    if (profile->legacy) {
        brls::Logger::warning("Rejecting QR sign-in: profile {} is legacy", profile->id);
        showFailure("akira/pair/mode_mismatch"_i18n);
        return;
    }

    PairView::adoptSignedInUser(profile->id, user);
    brls::Logger::info("Signed in to PSN with a QR code");
    brls::Application::notify(brls::getStr("akira/qr_sign_in/signed_in_fmt", user->online_id()));
    brls::Application::popActivity();
}

void QrSignInView::showCode(std::string_view approvalUrl) {
    if (tile) {
        if (qrView)
            tile->removeView(qrView);
        qrView = akira::ui::QrCodeView::make(approvalUrl, qrSide);
        if (qrView) {
            tile->setBackgroundColor(nvgRGB(255, 255, 255));
            if (placeholder)
                placeholder->setVisibility(brls::Visibility::GONE);
            tile->addView(qrView);
        }
    }
}

void QrSignInView::clearCode() {
    const auto& palette = akira::ui::active();
    deadline.reset();
    if (tile) {
        if (qrView)
            tile->removeView(qrView);
        tile->setBackgroundColor(palette.surfaceElevated);
    }
    qrView = nullptr;
    if (placeholder)
        placeholder->setVisibility(brls::Visibility::VISIBLE);
    if (timerLabel)
        timerLabel->setText("");
}

void QrSignInView::showStatus(const std::string& text, NVGcolor color) {
    if (!statusLabel)
        return;
    statusLabel->setText(text);
    statusLabel->setTextColor(color);
}

void QrSignInView::showFailure(const std::string& text) {
    failureShown = true;
    showStatus(text, akira::ui::active().danger);
    if (placeholder)
        placeholder->setText("akira/qr_sign_in/placeholder_failed"_i18n);
    if (retryButton) {
        retryButton->setVisibility(brls::Visibility::VISIBLE);
        brls::Application::giveFocus(retryButton);
    }
}

void QrSignInView::draw(NVGcontext* vg, float x, float y, float width, float height,
                        brls::Style style, brls::FrameContext* ctx) {
    if (deadline && timerLabel) {
        const auto left = std::chrono::duration_cast<std::chrono::seconds>(
            *deadline - std::chrono::steady_clock::now()).count();
        const int remaining = left < 0 ? 0 : static_cast<int>(left);
        if (remaining != lastShownSecond) {
            lastShownSecond = remaining;
            timerLabel->setText(brls::getStr("akira/qr_sign_in/timer_fmt",
                std::format("{}:{:02}", remaining / 60, remaining % 60)));
        }
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

auto QrSignInView::failureKey(SignInErrorKind kind) -> const char* {
    switch (kind) {
        case SignInErrorKind::declined:
            return "akira/qr_sign_in/status_declined";
        case SignInErrorKind::expired:
            return "akira/qr_sign_in/status_expired";
        case SignInErrorKind::transport:
            return "akira/qr_sign_in/status_offline";
        case SignInErrorKind::rate_limited:
            return "akira/qr_sign_in/status_rate_limited";
        case SignInErrorKind::verification_required:
            return "akira/qr_sign_in/status_verification";
        case SignInErrorKind::layout_changed:
            return "akira/qr_sign_in/status_layout_changed";
        case SignInErrorKind::cancelled:
            return "akira/qr_sign_in/status_cancelled";
        default:
            return "akira/qr_sign_in/status_failed";
    }
}
