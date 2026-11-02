#ifndef AKIRA_PAIR_VIEW_HPP
#define AKIRA_PAIR_VIEW_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

#include <borealis.hpp>

#include "core/pair_advertiser.hpp"
#include "core/pair_listener.hpp"
#include "psn/accounts.hpp"

class PairView : public brls::Box {
public:
    explicit PairView(bool createProfile = false);
    ~PairView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }
    static brls::View* create();
    static void adoptSignedInUser(int64_t profileId, const psn::Accounts::User& user);

private:
    brls::Label* codeLabel = nullptr;
    brls::Label* addrIpLabel = nullptr;
    brls::Label* addrPortLabel = nullptr;
    brls::Label* timerLabel = nullptr;
    brls::Label* statusLabel = nullptr;
    brls::Button* closeBtn = nullptr;

    akira::pair::PairListener listener;
    akira::pair::PairAdvertiser advertiser;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    int listenPort = 0;
    bool createProfile = false;

    std::chrono::steady_clock::time_point windowStart;
    int lastShownSecond = -1;
    bool counting = false;

    void startListening();
    void onEvent(akira::pair::ListenerEvent event);
    static void applyCredentials(const akira::pair::PairedCredentials& creds, bool createProfile);
};

#endif
