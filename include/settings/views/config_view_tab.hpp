#ifndef AKIRA_CONFIG_VIEW_TAB_HPP
#define AKIRA_CONFIG_VIEW_TAB_HPP

#include <borealis.hpp>

class ConfigViewTab : public brls::Box {
public:
    ConfigViewTab();
    ~ConfigViewTab() override = default;

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, configContainer, "config/container");
    BRLS_BIND(brls::Button, revealBtn, "config/revealBtn");

    bool credentialsRevealed = false;

    void loadConfigFromFile();
    void addTomlLine(const std::string& line);
    void addLine(const std::string& text, bool isHeader = false);
    std::string censorValue(const std::string& value);
    std::string processLine(const std::string& line);
};

#endif
