#ifndef AKIRA_UI_HEADER_HPP
#define AKIRA_UI_HEADER_HPP

namespace brls {
class AppletFrame;
class View;
}

void decorateAkiraHeader(brls::AppletFrame* appletFrame);
void registerAkiraTabActions(brls::View* view);

#endif // AKIRA_UI_HEADER_HPP
