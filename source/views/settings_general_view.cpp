#include "views/settings_general_view.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <arpa/inet.h>

#include "core/version.hpp"
#include "core/discovery_manager.hpp"
#include "core/discovery_sweep.hpp"
#include "ui/theme.hpp"

using namespace brls::literals;

SettingsGeneralView::SettingsGeneralView() {
    this->inflateFromXMLRes("xml/settings/general.xml");

    settings = SettingsManager::getInstance();

    initLanguageSelector();
    initThemeSelector();
    rebuildSubnetsUI();
    initConnectionShowStagesToggle();
    initEnableThreadAffinityToggle();
    initHolepunchRetryToggle();
    initRequestIdrOnFecFailureToggle();
    initPacketLossMaxSlider();
    initVersionUnlock();
}

void SettingsGeneralView::initEnableThreadAffinityToggle() {
    bool currentValue = settings->getEnableThreadAffinity();

    enableThreadAffinityToggle->init(
        "akira/settings/thread_affinity"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setEnableThreadAffinity(isOn);
            settings->writeFile();
            brls::Logger::info("Thread affinity set to {} (requires restart)", isOn ? "true" : "false");
        }
    );
}

void SettingsGeneralView::initConnectionShowStagesToggle() {
    bool currentValue = settings->getConnectionShowStages();

    connectionShowStagesToggle->init(
        "akira/settings/connection_stages"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setConnectionShowStages(isOn);
            settings->writeFile();
        }
    );
}

void SettingsGeneralView::initHolepunchRetryToggle() {
    bool currentValue = settings->getHolepunchRetry();

    holepunchRetryToggle->init(
        "akira/settings/holepunch_retry"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setHolepunchRetry(isOn);
            settings->writeFile();
            brls::Logger::info("Holepunch retry set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsGeneralView::initRequestIdrOnFecFailureToggle() {
    bool currentValue = settings->getRequestIdrOnFecFailure();

    requestIdrOnFecFailureToggle->init(
        "akira/settings/request_idr"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setRequestIdrOnFecFailure(isOn);
            settings->writeFile();
        }
    );
}

void SettingsGeneralView::initPacketLossMaxSlider() {
    float currentValue = settings->getPacketLossMax();
    int currentPercent = static_cast<int>(currentValue * 100.0f);

    packetLossMaxSlider->detail->setWidth(60);
    packetLossMaxSlider->detail->setShrink(0);
    packetLossMaxSlider->init(
        "akira/settings/packet_loss_max"_i18n,
        currentValue,
        [this](float value) {
            int percent = static_cast<int>(value * 100.0f);
            settings->setPacketLossMax(value);
            packetLossMaxSlider->detail->setText(brls::getStr("akira/settings/percent_format", percent));
            settings->writeFile();
        }
    );
    packetLossMaxSlider->detail->setText(brls::getStr("akira/settings/percent_format", currentPercent));
    packetLossMaxSlider->slider->setDiscreteStep(0.05f);
}

void SettingsGeneralView::initLanguageSelector() {
    static const std::vector<std::string> localeCodes = {"", "en-US", "ru-RU", "zh-Hans"};

    std::vector<std::string> options = {
        "akira/settings/lang_system"_i18n,
        "akira/settings/lang_en"_i18n,
		"akira/settings/lang_ru"_i18n,
        "akira/settings/lang_zh_hans"_i18n,
    };

    std::string currentLocale = settings->getDebugLocale();
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
            settings->setDebugLocale(locale);
            settings->writeFile();
			
            auto* dialog = new brls::Dialog("akira/settings/lang_restart"_i18n);
            dialog->setCancelable(false);
            dialog->addButton("akira/common/ok"_i18n, []() { brls::Application::quit(); });
            dialog->open();
        }
    );
}

void SettingsGeneralView::initThemeSelector() {
    int count = akira::ui::themeCount();

    std::vector<std::string> options;
    std::vector<std::string> ids;
    for (int i = 0; i < count; i++) {
        const akira::ui::Palette& p = akira::ui::themeAt(i);
        options.emplace_back(brls::getStr(std::string(p.name)));
        ids.emplace_back(p.id);
    }

    std::string current = settings->getUiTheme();
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
            settings->setUiTheme(id);
            settings->writeFile();

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
    DiscoveryManager* dm = DiscoveryManager::getInstance();
    if (dm->isServiceEnabled()) {
        dm->setServiceEnabled(false);
        dm->setServiceEnabled(true);
    }
}

void SettingsGeneralView::rebuildSubnetsUI() {
    subnetsBox->clearViews();

    std::string localCidr = DiscoveryManager::getInstance()->getLocalSubnetCidr();
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
        label->setTextColor(akira::ui::active().text);
        defaultRow->addView(label);

        auto* tag = new brls::Label();
        tag->setText("akira/settings/this_network"_i18n);
        tag->setFontSize(14);
        tag->setTextColor(akira::ui::active().textDim);
        defaultRow->addView(tag);

        subnetsBox->addView(defaultRow);
    }

    std::vector<std::string> subnets = splitSubnets(settings->getDiscoverySubnets());

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
        label->setTextColor(akira::ui::active().text);
        row->addView(label);

        auto* remove = new brls::Label();
        remove->setText("akira/settings/remove"_i18n);
        remove->setFontSize(16);
        remove->setTextColor(akira::ui::active().danger);
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
    addLabel->setTextColor(akira::ui::active().accent);
    addRow->addView(addLabel);
    subnetsBox->addView(addRow);
}

void SettingsGeneralView::promptAddSubnet() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (text.empty())
                return;
            if (!akira::discovery::isValidSweepCidr(text)) {
                brls::sync([]() {
                    auto* dialog = new brls::Dialog("akira/settings/subnet_invalid"_i18n);
                    dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
                    dialog->open();
                });
                return;
            }
            std::string normalized = akira::discovery::normalizeSweepCidr(text);
            std::vector<std::string> subnets = splitSubnets(settings->getDiscoverySubnets());
            if (std::find(subnets.begin(), subnets.end(), normalized) != subnets.end())
                return;
            subnets.push_back(normalized);
            settings->setDiscoverySubnets(joinSubnets(subnets));
            settings->writeFile();
            restartDiscovery();
            rebuildSubnetsUI();
        },
        "akira/settings/discovery_subnets_prompt"_i18n,
        "akira/settings/discovery_subnets_placeholder"_i18n,
        64, "", 0);
}

void SettingsGeneralView::removeSubnet(const std::string& value) {
    std::vector<std::string> subnets = splitSubnets(settings->getDiscoverySubnets());
    for (auto it = subnets.begin(); it != subnets.end(); ++it) {
        if (*it == value) {
            subnets.erase(it);
            break;
        }
    }
    settings->setDiscoverySubnets(joinSubnets(subnets));
    settings->writeFile();
    restartDiscovery();
    rebuildSubnetsUI();
}

void SettingsGeneralView::initVersionUnlock() {
    lastPowerUserClick = std::chrono::steady_clock::now();

    versionLabel->setText(std::string("Akira ") + akira::version::string());

    versionLabel->registerClickAction([this](brls::View*) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPowerUserClick).count();

        if (elapsed > 3000) {
            powerUserClickCount = 0;
        }

        lastPowerUserClick = now;
        powerUserClickCount++;

        if (powerUserClickCount >= 7 && !settings->getPowerUserMenuUnlocked()) {
            settings->setPowerUserMenuUnlocked(true);
            settings->writeFile();
            brls::Application::notify("akira/settings/power_user_unlocked"_i18n);
            brls::Logger::info("Power User Menu unlocked");
            powerUserClickCount = 0;
        }

        return true;
    });
}
