#ifndef AKIRA_PROFILE_CARD_VIEW_HPP
#define AKIRA_PROFILE_CARD_VIEW_HPP

#include <borealis.hpp>
#include <memory>

#include "settings/settings_manager.hpp"

class ProfileCardView : public brls::Box {
public:
    ProfileCardView();
    ~ProfileCardView() override;

    void refresh();

private:
    BRLS_BIND(brls::Image, avatar, "profileCard/avatar");
    BRLS_BIND(brls::Box, trophyCol, "profileCard/trophyCol");
    BRLS_BIND(brls::Box, track, "profileCard/track");
    BRLS_BIND(brls::Rectangle, progress, "profileCard/progress");
    BRLS_BIND(brls::Label, onlineIdLabel, "profileCard/onlineId");
    BRLS_BIND(brls::Label, plusLabel, "profileCard/plus");
    BRLS_BIND(brls::Label, statusLabel, "profileCard/status");
    BRLS_BIND(brls::Label, levelLabel, "profileCard/level");
    BRLS_BIND(brls::Label, platLabel, "profileCard/plat");
    BRLS_BIND(brls::Label, goldLabel, "profileCard/gold");
    BRLS_BIND(brls::Label, silverLabel, "profileCard/silver");
    BRLS_BIND(brls::Label, bronzeLabel, "profileCard/bronze");
    BRLS_BIND(brls::Image, platImg, "profileCard/platImg");
    BRLS_BIND(brls::Image, goldImg, "profileCard/goldImg");
    BRLS_BIND(brls::Image, silverImg, "profileCard/silverImg");
    BRLS_BIND(brls::Image, bronzeImg, "profileCard/bronzeImg");

    SettingsManager* settings = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

#endif // AKIRA_PROFILE_CARD_VIEW_HPP
