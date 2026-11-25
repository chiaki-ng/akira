#include "consoles/views/account_setup_card.hpp"

#include <borealis/core/i18n.hpp>

#include "accounts/views/profile_setup_view.hpp"
#include "app/views/motion.hpp"
#include "app/views/theme.hpp"

namespace akira::consoles::views {

using namespace brls::literals;

AccountSetupCard::AccountSetupCard()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setWidth(300);
    this->setHeight(238);
    this->setPadding(20);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));
    this->setCornerRadius(14);
    this->setBorderColor(app::views::withAlpha(app::views::active().accent, 0x66));
    this->setBorderThickness(2);
    this->setFocusable(true);

    auto* glyph = new brls::Label();
    glyph->setText("\xEE\xA1\x93");
    glyph->setFontSize(64);
    glyph->setTextColor(app::views::active().accent);
    glyph->setMarginBottom(10);
    this->addView(glyph);

    auto* label = new brls::Label();
    label->setText("akira/hosts/setup_account"_i18n);
    label->setFontSize(22);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setTextColor(app::views::active().text);
    this->addView(label);

    auto* hint = new brls::Label();
    hint->setText("akira/hosts/setup_account_hint"_i18n);
    hint->setFontSize(15);
    hint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    hint->setTextColor(app::views::active().textMuted);
    hint->setMarginTop(6);
    this->addView(hint);

    this->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new accounts::views::ProfileSetupView()));
        return true;
    });
    app::views::motion::liftOnFocus(this, focusAnim);
}

}
