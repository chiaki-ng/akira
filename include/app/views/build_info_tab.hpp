#ifndef AKIRA_BUILD_INFO_TAB_HPP
#define AKIRA_BUILD_INFO_TAB_HPP

#include <borealis.hpp>

class BuildInfoTab : public brls::Box {
public:
    BuildInfoTab();
    ~BuildInfoTab() override = default;

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, infoContainer, "info/container");

    void loadBuildInfo();
    void addInfoRow(const std::string& text, bool isHeader = false);
};

#endif // AKIRA_BUILD_INFO_TAB_HPP
