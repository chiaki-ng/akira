#ifndef AKIRA_LEGACY_PROFILE_VIEW_HPP
#define AKIRA_LEGACY_PROFILE_VIEW_HPP

#include <borealis.hpp>

#include <string>

class LegacyProfileView : public brls::Box {
public:
    LegacyProfileView();

    brls::View* getDefaultFocus() override;

private:
    void buildFields();
    brls::Label* addField(const std::string& title, const std::string& placeholder,
                          const std::string& hint, NVGcolor hintColor,
                          std::function<void()> onSelect);
    void askAccountId();
    void askName();
    void create();

    std::string m_account_id;
    std::string m_name;

    brls::Label* m_account_value = nullptr;
    brls::Label* m_name_value = nullptr;
    brls::View* m_first_field = nullptr;

    BRLS_BIND(brls::Box, fields, "legacy/fields");
    BRLS_BIND(brls::Button, createBtn, "legacy/createBtn");
};

#endif // AKIRA_LEGACY_PROFILE_VIEW_HPP
