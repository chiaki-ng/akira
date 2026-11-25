#include "play/views/quit_dialog.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

namespace akira::play::views {

using namespace brls::literals;

}

namespace akira::play::views {

void quitApp(bool toHbmenu, const std::string& message) {
    auto* dialog = new brls::Dialog(message);
    dialog->setCancelable(false);
    dialog->addButton("akira/common/ok"_i18n, [toHbmenu]() {
        auto* container = new brls::Box();
        container->setJustifyContent(brls::JustifyContent::CENTER);
        container->setAlignItems(brls::AlignItems::CENTER);
        container->setBackgroundColor(brls::Application::getTheme().getColor("brls/background"));

        auto* hint = new brls::Label();
        hint->setFocusable(true);
        hint->setHideHighlight(true);
        hint->setFontSize(32.0f);
        hint->setText("akira/app/quitting"_i18n);
        container->addView(hint);

        brls::Application::pushActivity(new brls::Activity(container), brls::TransitionAnimation::NONE);
        brls::Application::getPlatform()->exitToHomeMode(!toHbmenu);
        brls::Application::quit();
    });
    dialog->open();
}

}
