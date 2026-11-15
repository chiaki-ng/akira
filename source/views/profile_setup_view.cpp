#include "views/profile_setup_view.hpp"

#include "core/settings_manager.hpp"
#include "util/custom_dns.hpp"
#include "ui/theme.hpp"
#include "views/custom_dns_prompt.hpp"
#include "views/legacy_profile_view.hpp"
#include "views/pair_view.hpp"
#include "views/qr_sign_in_view.hpp"

using namespace brls::literals;

ProfileSetupView::ProfileSetupView(bool createProfile, bool firstRun)
    : m_create_profile(createProfile), m_first_run(firstRun) {
    this->inflateFromXMLRes("xml/views/profile_setup.xml");

    const auto& palette = akira::ui::active();

    if (glyph) {
        glyph->setText("\xEE\xA1\x93");
        glyph->setTextColor(palette.accent);
    }

    buildOptions();

    this->registerAction("akira/common/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
    this->registerAction("akira/settings/set_dns"_i18n, brls::ControllerButton::BUTTON_Y,
        [](brls::View*) {
            CustomDnsPrompt::open([]() {
                const auto servers = SettingsManager::getInstance()->getCustomDnsServers();
                const std::string shown = servers.empty() ? "akira/settings/custom_dns_system"_i18n
                                                          : akira::net::CustomDns::formatServers(servers);
                brls::Application::notify(brls::getStr("akira/settings/custom_dns_saved", shown));
            });
            return true;
        });
}

void ProfileSetupView::buildOptions() {
    const auto& palette = akira::ui::active();
    const bool createProfile = m_create_profile;

    addOption("\xEE\x8C\xAC",
              "akira/profile_setup/qr_title"_i18n,
              "akira/profile_setup/qr_badge"_i18n, palette.accent,
              "akira/profile_setup/qr_body"_i18n,
              "akira/profile_setup/qr_note"_i18n,
              [createProfile]() {
                  brls::Application::pushActivity(
                      new brls::Activity(new QrSignInView(createProfile)));
              });

    addOption("\xEE\xA1\x93",
              "akira/profile_setup/companion_title"_i18n,
              "akira/profile_setup/companion_badge"_i18n, palette.accent,
              "akira/profile_setup/companion_body"_i18n,
              "akira/profile_setup/companion_note"_i18n,
              [createProfile]() {
                  brls::Application::pushActivity(
                      new brls::Activity(new PairView(createProfile)));
              });

    addOption("\xEE\xA0\x8C",
              "akira/profile_setup/legacy_title"_i18n,
              "akira/profile_setup/legacy_badge"_i18n, palette.warning,
              "akira/profile_setup/legacy_body"_i18n,
              "",
              []() {
                  brls::Application::pushActivity(
                      new brls::Activity(new LegacyProfileView()));
              });
}

void ProfileSetupView::addOption(const std::string& glyphText, const std::string& title,
                                 const std::string& badge, NVGcolor badgeColor,
                                 const std::string& body, const std::string& note,
                                 std::function<void()> onSelect) {
    if (!options)
        return;

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
    titleLabel->setText(glyphText + "  " + title);
    titleLabel->setFontSize(21.0f);
    titleLabel->setTextColor(palette.text);
    header->addView(titleLabel);

    auto* badgeLabel = new brls::Label();
    badgeLabel->setText(badge);
    badgeLabel->setFontSize(16.0f);
    badgeLabel->setTextColor(badgeColor);
    badgeLabel->setMarginLeft(16.0f);
    header->addView(badgeLabel);

    card->addView(header);

    auto* bodyLabel = new brls::Label();
    bodyLabel->setText(body);
    bodyLabel->setFontSize(15.0f);
    bodyLabel->setTextColor(palette.textMuted);
    card->addView(bodyLabel);

    if (!note.empty()) {
        auto* noteLabel = new brls::Label();
        noteLabel->setText(note);
        noteLabel->setFontSize(15.0f);
        noteLabel->setTextColor(palette.textDim);
        noteLabel->setMarginTop(6.0f);
        card->addView(noteLabel);
    }

    if (!m_first_option)
        m_first_option = card;

    options->addView(card);
}

void ProfileSetupView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    if (m_first_run && !SettingsManager::getInstance()->getProfiles().empty())
        brls::Application::popActivity();
}

brls::View* ProfileSetupView::getDefaultFocus() {
    return m_first_option ? m_first_option->getDefaultFocus() : brls::Box::getDefaultFocus();
}
