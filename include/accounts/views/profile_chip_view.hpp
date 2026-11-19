#ifndef AKIRA_PROFILE_CHIP_VIEW_HPP
#define AKIRA_PROFILE_CHIP_VIEW_HPP

#include <borealis.hpp>
#include <memory>

#include "settings/settings_manager.hpp"

class ProfileChipView : public brls::Box {
public:
    ProfileChipView();
    ~ProfileChipView() override;

    void refresh();

private:
    BRLS_BIND(brls::Image, avatar, "chip/avatar");
    BRLS_BIND(brls::Label, nameLabel, "chip/name");
    BRLS_BIND(brls::Box, plusBadge, "chip/plusBadge");
    BRLS_BIND(brls::Label, plusLabel, "chip/plus");
    BRLS_BIND(brls::Box, tagBox, "chip/tag");
    BRLS_BIND(brls::Label, tagLabel, "chip/tagLabel");
    BRLS_BIND(brls::Box, dot, "chip/dot");
    BRLS_BIND(brls::Label, cloudLabel, "chip/cloud");

    SettingsManager* settings = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

#endif // AKIRA_PROFILE_CHIP_VIEW_HPP
