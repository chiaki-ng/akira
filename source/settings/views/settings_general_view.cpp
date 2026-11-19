#include "settings/views/settings_general_view.hpp"
#include "app/view_services.hpp"
#include "network/views/custom_dns_prompt.hpp"
#include "log/log.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <arpa/inet.h>

#include "app/version.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/discovery_sweep.hpp"
#include "app/views/theme.hpp"
#include "network/custom_dns.hpp"

namespace akira::settings::views {

using namespace brls::literals;

SettingsGeneralView::SettingsGeneralView() {
    this->inflateFromXMLRes("xml/settings/general.xml");

    services = &app::ViewServices::current();

    initLanguageSelector();
    initThemeSelector();
    initCustomDnsCell();
    rebuildSubnetsUI();
    initConnectionShowStagesToggle();
    initHolepunchRetryToggle();
    initRequestIdrOnFecFailureToggle();
    initPacketLossMaxSlider();
    initVersionUnlock();
}

void SettingsGeneralView::initConnectionShowStagesToggle() {
    bool currentValue = services->appSettings.connectionShowStages();

    connectionShowStagesToggle->init(
        "akira/settings/connection_stages"_i18n,
        currentValue,
        [this](bool isOn) {
            services->appSettings.setConnectionShowStages(isOn);
            services->store.save();
        }
    );
}

void SettingsGeneralView::initHolepunchRetryToggle() {
    bool currentValue = services->networkSettings.holepunchRetry();

    holepunchRetryToggle->init(
        "akira/settings/holepunch_retry"_i18n,
        currentValue,
        [this](bool isOn) {
            services->networkSettings.setHolepunchRetry(isOn);
            services->store.save();
            log::info("Holepunch retry set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsGeneralView::initRequestIdrOnFecFailureToggle() {
    bool currentValue = services->stream.requestIdrOnFecFailure();

    requestIdrOnFecFailureToggle->init(
        "akira/settings/request_idr"_i18n,
        currentValue,
        [this](bool isOn) {
            services->stream.setRequestIdrOnFecFailure(isOn);
            services->store.save();
        }
    );
}

void SettingsGeneralView::initPacketLossMaxSlider() {
    float currentValue = services->stream.packetLossMax();
    int currentPercent = static_cast<int>(currentValue * 100.0f);

    packetLossMaxSlider->detail->setWidth(60);
    packetLossMaxSlider->detail->setShrink(0);
    packetLossMaxSlider->init(
        "akira/settings/packet_loss_max"_i18n,
        currentValue,
        [this](float value) {
            int percent = static_cast<int>(value * 100.0f);
            services->stream.setPacketLossMax(value);
            packetLossMaxSlider->detail->setText(brls::getStr("akira/settings/percent_format", percent));
            services->store.save();
        }
    );
    packetLossMaxSlider->detail->setText(brls::getStr("akira/settings/percent_format", currentPercent));
    packetLossMaxSlider->slider->setDiscreteStep(0.05f);
}

void SettingsGeneralView::initLanguageSelector() {
    static const std::vector<std::string> localeCodes = {"", "en-US", "zh-Hans"};

    std::vector<std::string> options = {
        "akira/settings/lang_system"_i18n,
        "akira/settings/lang_en"_i18n,
        "akira/settings/lang_zh_hans"_i18n,
    };

    std::string currentLocale = services->appSettings.debugLocale();
    int currentIndex = 0;
    for (size_t i = 1; i < localeCodes.size(); i++) {
        if (localeCodes[i] == currentLocale) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    languageSelector->init(
        "akira/settings/language"_i18n,
        options,
        currentIndex,
        [](int selected) {},
        [this](int selected) {
            std::string locale = (selected > 0 && selected < (int)localeCodes.size()) ? localeCodes[selected] : "";
            services->appSettings.setDebugLocale(locale);
            services->store.save();
        }
    );
}

void SettingsGeneralView::initThemeSelector() {
    int count = app::views::themeCount();

    std::vector<std::string> options;
    std::vector<std::string> ids;
    for (int i = 0; i < count; i++) {
        const app::views::Palette& p = app::views::themeAt(i);
        options.emplace_back(p.name);
        ids.emplace_back(p.id);
    }

    std::string current = services->appSettings.uiTheme();
    int currentIndex = 0;
    for (size_t i = 0; i < ids.size(); i++) {
        if (ids[i] == current) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    themeSelector->init(
        "akira/settings/theme"_i18n,
        options,
        currentIndex,
        [](int) {},
        [this, ids](int selected) {
            std::string id = (selected >= 0 && selected < static_cast<int>(ids.size()))
                                 ? ids[selected]
                                 : std::string("playstation");
            services->appSettings.setUiTheme(id);
            services->store.save();

            auto* dialog = new brls::Dialog("akira/settings/theme_restart"_i18n);
            dialog->setCancelable(false);
            dialog->addButton("akira/common/ok"_i18n, []() { brls::Application::quit(); });
            dialog->open();
        });
}

static std::vector<std::string> splitSubnets(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find_first_of(", \t\n", start);
        std::string tok = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = (end == std::string::npos) ? s.size() : end + 1;
        if (!tok.empty())
            out.push_back(tok);
    }
    return out;
}

static std::string joinSubnets(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0)
            out += ", ";
        out += v[i];
    }
    return out;
}

static void restartDiscovery() {
    consoles::DiscoveryManager* dm = &app::ViewServices::current().discovery;
    if (dm->isServiceEnabled()) {
        dm->setServiceEnabled(false);
        dm->setServiceEnabled(true);
    }
}

void SettingsGeneralView::scheduleSubnetsRebuild() {
    brls::sync([this]() {
        brls::Application::giveFocus(customDnsCell);
        rebuildSubnetsUI();
        if (addSubnetRow)
            brls::Application::giveFocus(addSubnetRow);
    });
}

void SettingsGeneralView::rebuildSubnetsUI() {
    subnetsBox->clearViews();

    std::string localCidr = app::ViewServices::current().discovery.getLocalSubnetCidr();
    if (!localCidr.empty()) {
        auto* defaultRow = new brls::Box();
        defaultRow->setAxis(brls::Axis::ROW);
        defaultRow->setAlignItems(brls::AlignItems::CENTER);
        defaultRow->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
        defaultRow->setHeight(56);
        defaultRow->setPaddingLeft(15);
        defaultRow->setPaddingRight(15);

        auto* label = new brls::Label();
        label->setText(localCidr);
        label->setFontSize(18);
        label->setTextColor(app::views::active().text);
        defaultRow->addView(label);

        auto* tag = new brls::Label();
        tag->setText("akira/settings/this_network"_i18n);
        tag->setFontSize(14);
        tag->setTextColor(app::views::active().textDim);
        defaultRow->addView(tag);

        subnetsBox->addView(defaultRow);
    }

    std::vector<std::string> subnets = splitSubnets(services->networkSettings.discoverySubnets());

    for (const std::string& subnet : subnets) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setJustifyContent(brls::JustifyContent::SPACE_BETWEEN);
        row->setHeight(56);
        row->setPaddingLeft(15);
        row->setPaddingRight(15);
        row->setCornerRadius(8);
        row->setFocusable(true);

        std::string value = subnet;
        row->registerClickAction([this, value](brls::View*) {
            removeSubnet(value);
            return true;
        });

        auto* label = new brls::Label();
        label->setText(subnet);
        label->setFontSize(18);
        label->setTextColor(app::views::active().text);
        row->addView(label);

        auto* remove = new brls::Label();
        remove->setText("akira/settings/remove"_i18n);
        remove->setFontSize(16);
        remove->setTextColor(app::views::active().danger);
        row->addView(remove);

        subnetsBox->addView(row);
    }

    auto* addRow = new brls::Box();
    addRow->setAxis(brls::Axis::ROW);
    addRow->setAlignItems(brls::AlignItems::CENTER);
    addRow->setHeight(56);
    addRow->setPaddingLeft(15);
    addRow->setPaddingRight(15);
    addRow->setCornerRadius(8);
    addRow->setFocusable(true);
    addRow->registerClickAction([this](brls::View*) {
        promptAddSubnet();
        return true;
    });

    auto* addLabel = new brls::Label();
    addLabel->setText("akira/settings/add_subnet"_i18n);
    addLabel->setFontSize(18);
    addLabel->setTextColor(app::views::active().accent);
    addRow->addView(addLabel);
    subnetsBox->addView(addRow);
    addSubnetRow = addRow;
}

void SettingsGeneralView::initCustomDnsCell() {
    customDnsCell->setText("akira/settings/custom_dns"_i18n);
    refreshCustomDnsCell();
    customDnsCell->registerClickAction([this](brls::View*) {
        promptCustomDns();
        return true;
    });
}

void SettingsGeneralView::refreshCustomDnsCell() {
    const auto servers = services->networkSettings.customDnsServers();
    customDnsCell->setDetailText(servers.empty() ? "akira/settings/custom_dns_system"_i18n
                                                 : network::CustomDns::formatServers(servers));
}

void SettingsGeneralView::promptCustomDns() {
    network::views::CustomDnsPrompt::open([this]() { refreshCustomDnsCell(); });
}

void SettingsGeneralView::promptAddSubnet() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (text.empty())
                return;
            if (!consoles::isValidSweepCidr(text)) {
                brls::sync([]() {
                    auto* dialog = new brls::Dialog("akira/settings/subnet_invalid"_i18n);
                    dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
                    dialog->open();
                });
                return;
            }
            std::string normalized = consoles::normalizeSweepCidr(text);
            std::vector<std::string> subnets = splitSubnets(services->networkSettings.discoverySubnets());
            if (std::find(subnets.begin(), subnets.end(), normalized) != subnets.end())
                return;
            subnets.push_back(normalized);
            services->networkSettings.setDiscoverySubnets(joinSubnets(subnets));
            services->store.save();
            restartDiscovery();
            scheduleSubnetsRebuild();
        },
        "akira/settings/discovery_subnets_prompt"_i18n,
        "akira/settings/discovery_subnets_placeholder"_i18n,
        64, "", 0);
}

void SettingsGeneralView::removeSubnet(const std::string& value) {
    std::vector<std::string> subnets = splitSubnets(services->networkSettings.discoverySubnets());
    for (auto it = subnets.begin(); it != subnets.end(); ++it) {
        if (*it == value) {
            subnets.erase(it);
            break;
        }
    }
    services->networkSettings.setDiscoverySubnets(joinSubnets(subnets));
    services->store.save();
    restartDiscovery();
    scheduleSubnetsRebuild();
}

void SettingsGeneralView::initVersionUnlock() {
    lastPowerUserClick = std::chrono::steady_clock::now();

    versionLabel->setText(std::string("Akira ") + app::version::string());

    versionLabel->registerClickAction([this](brls::View*) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPowerUserClick).count();

        if (elapsed > 3000) {
            powerUserClickCount = 0;
        }

        lastPowerUserClick = now;
        powerUserClickCount++;

        if (powerUserClickCount >= 7 && !services->developer.powerUserMenuUnlocked()) {
            services->developer.setPowerUserMenuUnlocked(true);
            services->store.save();
            brls::Application::notify("akira/settings/power_user_unlocked"_i18n);
            log::info("Power User Menu unlocked");
            powerUserClickCount = 0;
        }

        return true;
    });
}

}
