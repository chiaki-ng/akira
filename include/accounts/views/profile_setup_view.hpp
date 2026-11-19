#ifndef AKIRA_PROFILE_SETUP_VIEW_HPP
#define AKIRA_PROFILE_SETUP_VIEW_HPP

#include <borealis.hpp>

class ProfileSetupView : public brls::Box {
public:
    explicit ProfileSetupView(bool createProfile = false, bool firstRun = false);

    void willAppear(bool resetState) override;
    brls::View* getDefaultFocus() override;

private:
    void buildOptions();
    void addOption(const std::string& glyph, const std::string& title,
                   const std::string& badge, NVGcolor badgeColor, const std::string& body,
                   const std::string& note, std::function<void()> onSelect);

    bool m_create_profile;
    bool m_first_run;
    brls::View* m_first_option = nullptr;

    BRLS_BIND(brls::Label, glyph, "setup/glyph");
    BRLS_BIND(brls::Box, options, "setup/options");
};

#endif // AKIRA_PROFILE_SETUP_VIEW_HPP
