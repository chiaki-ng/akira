#ifndef AKIRA_QR_SIGN_IN_VIEW_HPP
#define AKIRA_QR_SIGN_IN_VIEW_HPP

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include <borealis.hpp>

#include "psn/accounts.hpp"

class QrSignInView : public brls::Box {
public:
    explicit QrSignInView(bool createProfile = false);
    ~QrSignInView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override;

    static void openIfSignInExpired();

private:
    void start();
    void onProgress(const libchiaking::users::QrSignInState& state);
    void onSignedIn(psn::SignInOutcome outcome, const psn::Accounts::User& user);
    void showCode(std::string_view approvalUrl);
    void clearCode();
    void showStatus(const std::string& text, NVGcolor color);
    void showFailure(const std::string& text);
    static auto failureKey(libchiaking::users::SignInErrorKind kind) -> const char*;

    static constexpr float qrSide = 340.0f;

    brls::Box* tile = nullptr;
    brls::Label* placeholder = nullptr;
    brls::Label* timerLabel = nullptr;
    brls::Label* statusLabel = nullptr;
    brls::Button* retryButton = nullptr;
    brls::Button* closeButton = nullptr;
    brls::View* qrView = nullptr;

    bool createProfile = false;
    bool failureShown = false;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    int lastShownSecond = -1;
};

#endif
