#ifndef AKIRA_UI_HEADER_HPP
#define AKIRA_UI_HEADER_HPP

#include <string>

namespace brls {
class AppletFrame;
class View;
}

namespace akira::app::views {

class AkiraHeader {
public:
    static void decorate(brls::AppletFrame* appletFrame);
    static void registerTabActions(brls::View* view);
    static void openTrophies();
    static void openSettings();

private:
    static std::string appVersion();
};

}

#endif // AKIRA_UI_HEADER_HPP
