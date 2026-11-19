#ifndef AKIRA_PROFILE_SWITCHER_VIEW_HPP
#define AKIRA_PROFILE_SWITCHER_VIEW_HPP

#include <borealis.hpp>
#include <functional>
#include <string>

#include "accounts/profile.hpp"
#include "settings/settings_manager.hpp"
#include "accounts/views/profile_card_view.hpp"

namespace akira::accounts::views {

class ProfileSwitcherView : public brls::Box {
public:
    ProfileSwitcherView();

    void refresh();

    std::function<void()> onProfileChanged;

private:
    void setExpanded(bool value);
    void rebuildRows();
    float measurePanel() const;

    brls::Box* makeProfileRow(const Profile& profile, bool isActive);
    brls::Box* makeAddRow();
    brls::Box* makeAvatar(const std::string& label);

    void selectProfile(int64_t id);
    void confirmRemoveProfile(int64_t id, const std::string& label);
    void addProfileFlow();

    settings::SettingsManager* settings = nullptr;
    ProfileCardView* card = nullptr;
    brls::Box* header = nullptr;
    brls::Box* panel = nullptr;
    brls::Label* chevron = nullptr;

    bool expanded = false;
    brls::Animatable heightAnim{0.0f};
};

}

#endif // AKIRA_PROFILE_SWITCHER_VIEW_HPP
