#include "views/settings_debug_view.hpp"
#include "views/discovery_log_view.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

using namespace brls::literals;

SettingsDebugView::SettingsDebugView() {
    this->inflateFromXMLRes("xml/settings/debug_logging.xml");

    settings = SettingsManager::getInstance();

    initSilenceAllLogsToggle();
    initEnableFileLoggingToggle();
    initDebugLwipLogToggle();
    initDebugWireguardLogToggle();
    initDebugRenderLogToggle();
    initChiakiLogLevelSelector();
    initDebugDiscoveryLogToggle();
    initDebugFfmpegLogToggle();

    openDiscoveryLogBtn->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new DiscoveryLogView()));
        return true;
    });
}

void SettingsDebugView::initSilenceAllLogsToggle() {
    silenceAllLogsToggle->init(
        "akira/settings/silence_all_logs"_i18n,
        settings->getSilenceAllLogs(),
        [this](bool isOn) {
            settings->setSilenceAllLogs(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initEnableFileLoggingToggle() {
    bool currentValue = settings->getEnableFileLogging();

    enableFileLoggingToggle->init(
        "akira/settings/file_logging"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setEnableFileLogging(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initDebugLwipLogToggle() {
    bool currentValue = settings->getDebugLwipLog();

    debugLwipLogToggle->init(
        "akira/settings/lwip_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setDebugLwipLog(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initDebugWireguardLogToggle() {
    bool currentValue = settings->getDebugWireguardLog();

    debugWireguardLogToggle->init(
        "akira/settings/wireguard_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setDebugWireguardLog(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initDebugRenderLogToggle() {
    bool currentValue = settings->getDebugRenderLog();

    debugRenderLogToggle->init(
        "akira/settings/render_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setDebugRenderLog(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initChiakiLogLevelSelector() {
    const std::vector<std::string> options = {
        "akira/settings/chiaki_log_normal"_i18n,
        "akira/settings/chiaki_log_info"_i18n,
        "akira/settings/chiaki_log_debug"_i18n,
        "akira/settings/chiaki_log_trace"_i18n,
    };
    const int current = static_cast<int>(settings->getChiakiLogVerbosity());

    chiakiLogLevelSelector->init(
        "akira/settings/chiaki_log"_i18n,
        options,
        current,
        [](int) {},
        [this](int selected) {
            if (selected < static_cast<int>(ChiakiLogVerbosity::Normal) ||
                selected > static_cast<int>(ChiakiLogVerbosity::Trace))
                selected = static_cast<int>(ChiakiLogVerbosity::Normal);
            settings->setChiakiLogVerbosity(static_cast<ChiakiLogVerbosity>(selected));
            settings->writeFile();
        });
}

void SettingsDebugView::initDebugDiscoveryLogToggle() {
    bool currentValue = settings->getDebugDiscoveryLog();

    debugDiscoveryLogToggle->init(
        "akira/settings/discovery_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setDebugDiscoveryLog(isOn);
            settings->writeFile();
        }
    );
}

void SettingsDebugView::initDebugFfmpegLogToggle() {
    bool currentValue = settings->getDebugFfmpegLog();

    debugFfmpegLogToggle->init(
        "akira/settings/ffmpeg_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setDebugFfmpegLog(isOn);
            settings->writeFile();
        }
    );
}
