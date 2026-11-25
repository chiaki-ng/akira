#pragma once

#include <string>

#include <borealis.hpp>

namespace akira::app::views {

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    void onResume() override;
    void onContentAvailable() override;

private:
    static std::string localIpAddress();

    void addFooterStatus(brls::Box* rowBox);
    void showDiscoveryStatus();

    brls::Label* discoveryStatusLabel = nullptr;
    brls::RepeatingTimer discoveryStatusTimer;
};

}
