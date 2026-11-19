#include "settings/views/settings_debug_view.hpp"
#include "consoles/views/discovery_log_view.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

namespace akira::settings::views {

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
        brls::Application::pushActivity(new brls::Activity(new consoles::views::DiscoveryLogView()));
        return true;
    });
}

void SettingsDebugView::initSilenceAllLogsToggle() {
    silenceAllLogsToggle->init(
        "akira/settings/silence_all_logs"_i18n,
        settings->developer().silenceAllLogs(),
        [this](bool isOn) {
            settings->developer().setSilenceAllLogs(isOn);
            settings->store().save();
        }
    );
}

void SettingsDebugView::initEnableFileLoggingToggle() {
    bool currentValue = settings->developer().fileLogging();

    enableFileLoggingToggle->init(
        "akira/settings/file_logging"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setFileLogging(isOn);
            settings->store().save();
        }
    );
}

void SettingsDebugView::initDebugLwipLogToggle() {
    bool currentValue = settings->developer().lwipLog();

    debugLwipLogToggle->init(
        "akira/settings/lwip_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setLwipLog(isOn);
            settings->store().save();
        }
    );
}

void SettingsDebugView::initDebugWireguardLogToggle() {
    bool currentValue = settings->developer().wireguardLog();

    debugWireguardLogToggle->init(
        "akira/settings/wireguard_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setWireguardLog(isOn);
            settings->store().save();
        }
    );
}

void SettingsDebugView::initDebugRenderLogToggle() {
    bool currentValue = settings->developer().renderLog();

    debugRenderLogToggle->init(
        "akira/settings/render_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setRenderLog(isOn);
            settings->store().save();
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
    const int current = static_cast<int>(settings->developer().chiakiLogVerbosity());

    chiakiLogLevelSelector->init(
        "akira/settings/chiaki_log"_i18n,
        options,
        current,
        [](int) {},
        [this](int selected) {
            if (selected < static_cast<int>(ChiakiLogVerbosity::Normal) ||
                selected > static_cast<int>(ChiakiLogVerbosity::Trace))
                selected = static_cast<int>(ChiakiLogVerbosity::Normal);
            settings->developer().setChiakiLogVerbosity(static_cast<ChiakiLogVerbosity>(selected));
            settings->store().save();
        });
}

void SettingsDebugView::initDebugDiscoveryLogToggle() {
    bool currentValue = settings->developer().discoveryLog();

    debugDiscoveryLogToggle->init(
        "akira/settings/discovery_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setDiscoveryLog(isOn);
            settings->store().save();
        }
    );
}

void SettingsDebugView::initDebugFfmpegLogToggle() {
    bool currentValue = settings->developer().ffmpegLog();

    debugFfmpegLogToggle->init(
        "akira/settings/ffmpeg_log"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->developer().setFfmpegLog(isOn);
            settings->store().save();
        }
    );
}

}
