#include "consoles/views/add_host_card.hpp"

#include <borealis/core/i18n.hpp>

#include "app/views/motion.hpp"
#include "app/views/theme.hpp"
#include "consoles/views/add_host_tab.hpp"

namespace akira::consoles::views {

using namespace brls::literals;

AddHostCard::AddHostCard()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setWidth(174);
    this->setHeight(238);
    this->setPadding(14);
    this->setMarginRight(14);
    this->setMarginBottom(14);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(14);
    this->setBorderColor(app::views::withAlpha(app::views::active().accent, 0x66));
    this->setBorderThickness(2);
    this->setFocusable(true);

    auto* plus = new brls::Label();
    plus->setText("+");
    plus->setFontSize(64);
    plus->setTextColor(app::views::active().accent);
    plus->setMarginBottom(6);
    this->addView(plus);

    auto* label = new brls::Label();
    label->setText("akira/add_host/add_console"_i18n);
    label->setFontSize(22);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setTextColor(app::views::active().textMuted);
    this->addView(label);

    this->registerClickAction([](brls::View*) {
        openAddHostFlow();
        return true;
    });
    app::views::motion::liftOnFocus(this, focusAnim);
}

void AddHostCard::openAddHostFlow()
{
    auto* view = AddHostTab::create();
    view->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    }, false);
    auto* frame = new brls::AppletFrame(view);
    frame->setTitle("akira/tabs/add_host"_i18n);
    brls::Application::pushActivity(new brls::Activity(frame));
}

}
