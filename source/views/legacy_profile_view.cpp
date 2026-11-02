#include "views/legacy_profile_view.hpp"

#include <libchiaking/users_types.hpp>

#include "core/profile.hpp"
#include "core/settings_manager.hpp"
#include "ui/theme.hpp"

using namespace brls::literals;

namespace {

constexpr int kAccountIdMaxLength = 32;
constexpr int kNameMaxLength = 32;

}

LegacyProfileView::LegacyProfileView() {
    this->inflateFromXMLRes("xml/views/legacy_profile.xml");

    buildFields();

    if (createBtn) {
        createBtn->registerClickAction([this](brls::View*) {
            create();
            return true;
        });
    }

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
}

void LegacyProfileView::buildFields() {
    if (!fields)
        return;

    const auto& palette = akira::ui::active();

    m_account_value = addField("akira/profile_setup/account_id"_i18n,
                               "akira/profile_setup/account_id_placeholder"_i18n,
                               "akira/profile_setup/account_id_hint"_i18n, palette.warning,
                               [this]() { askAccountId(); });

    m_name_value = addField("akira/profile_setup/name"_i18n,
                            "akira/profile_setup/name_optional"_i18n,
                            "akira/profile_setup/name_hint"_i18n, palette.textDim,
                            [this]() { askName(); });
}

brls::Label* LegacyProfileView::addField(const std::string& title, const std::string& placeholder,
                                         const std::string& hint, NVGcolor hintColor,
                                         std::function<void()> onSelect) {
    const auto& palette = akira::ui::active();

    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setWidth(brls::View::AUTO);
    card->setHeight(brls::View::AUTO);
    card->setBackgroundColor(palette.surface);
    card->setCornerRadius(10.0f);
    card->setBorderColor(palette.surfaceLine);
    card->setBorderThickness(1.0f);
    card->setMarginBottom(14.0f);
    card->setPadding(18.0f, 22.0f, 18.0f, 22.0f);
    card->setFocusable(true);
    card->setHighlightCornerRadius(10.0f);
    card->registerClickAction([onSelect](brls::View*) {
        onSelect();
        return true;
    });

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setWidth(brls::View::AUTO);
    header->setHeight(brls::View::AUTO);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
    header->setMarginBottom(10.0f);

    auto* titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(21.0f);
    titleLabel->setTextColor(palette.text);
    header->addView(titleLabel);

    auto* valueLabel = new brls::Label();
    valueLabel->setText(placeholder);
    valueLabel->setFontSize(19.0f);
    valueLabel->setTextColor(palette.textDim);
    valueLabel->setMarginLeft(16.0f);
    header->addView(valueLabel);

    card->addView(header);

    auto* hintLabel = new brls::Label();
    hintLabel->setText(hint);
    hintLabel->setFontSize(15.0f);
    hintLabel->setTextColor(hintColor);
    card->addView(hintLabel);

    if (!m_first_field)
        m_first_field = card;

    fields->addView(card);
    return valueLabel;
}

void LegacyProfileView::askAccountId() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (!text.empty() && !libchiaking::AccountId::from_base64(text)) {
                brls::Application::notify("akira/pair/status_bad_payload"_i18n);
                return;
            }
            m_account_id = text;
            if (m_account_value) {
                const auto& palette = akira::ui::active();
                m_account_value->setText(
                    text.empty() ? "akira/profile_setup/account_id_placeholder"_i18n : text);
                m_account_value->setTextColor(text.empty() ? palette.textDim : palette.accent);
            }
        },
        "akira/profile_setup/account_id"_i18n, "akira/profile_setup/account_id_hint"_i18n,
        kAccountIdMaxLength, m_account_id, 0);
}

void LegacyProfileView::askName() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            m_name = text;
            if (m_name_value) {
                const auto& palette = akira::ui::active();
                m_name_value->setText(
                    text.empty() ? "akira/profile_setup/name_optional"_i18n : text);
                m_name_value->setTextColor(text.empty() ? palette.textDim : palette.accent);
            }
        },
        "akira/profile_setup/name"_i18n, "akira/profile_setup/name_hint"_i18n, kNameMaxLength,
        m_name, 0);
}

void LegacyProfileView::create() {
    if (!libchiaking::AccountId::from_base64(m_account_id)) {
        brls::Application::notify("akira/pair/status_bad_payload"_i18n);
        return;
    }

    auto* settings = SettingsManager::getInstance();

    Profile profile;
    profile.legacy = true;
    profile.accountId = m_account_id;
    profile.onlineId = m_name;

    const int64_t id = settings->addProfile(profile);
    settings->setActiveProfileId(id);
    settings->refreshLegacyGate();
    settings->writeFile();

    brls::Application::notify("akira/profile_setup/created"_i18n);
    brls::Application::popActivity();
}

brls::View* LegacyProfileView::getDefaultFocus() {
    return m_first_field ? m_first_field->getDefaultFocus() : brls::Box::getDefaultFocus();
}
