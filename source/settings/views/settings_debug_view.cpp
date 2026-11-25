#include "settings/views/settings_debug_view.hpp"
#include "app/view_services.hpp"
#include "consoles/views/discovery_log_view.hpp"

#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

namespace akira::settings::views {

using namespace brls::literals;

SettingsDebugView::SettingsDebugView() {
    this->inflateFromXMLRes("xml/settings/debug_logging.xml");

    services = &app::ViewServices::current();

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
        services->developer.silenceAllLogs(),
        [this](bool isOn) {
            services->developer.setSilenceAllLogs(isOn);
            services->store.save();
        }
    );
}

void SettingsDebugView::initEnableFileLoggingToggle() {
    bool currentValue = services->developer.fileLogging();

    enableFileLoggingToggle->init(
        "akira/settings/file_logging"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setFileLogging(isOn);
            services->store.save();
        }
    );
}

void SettingsDebugView::initDebugLwipLogToggle() {
    bool currentValue = services->developer.lwipLog();

    debugLwipLogToggle->init(
        "akira/settings/lwip_log"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setLwipLog(isOn);
            services->store.save();
        }
    );
}

void SettingsDebugView::initDebugWireguardLogToggle() {
    bool currentValue = services->developer.wireguardLog();

    debugWireguardLogToggle->init(
        "akira/settings/wireguard_log"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setWireguardLog(isOn);
            services->store.save();
        }
    );
}

void SettingsDebugView::initDebugRenderLogToggle() {
    bool currentValue = services->developer.renderLog();

    debugRenderLogToggle->init(
        "akira/settings/render_log"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setRenderLog(isOn);
            services->store.save();
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
    const int current = static_cast<int>(services->developer.chiakiLogVerbosity());

    chiakiLogLevelSelector->init(
        "akira/settings/chiaki_log"_i18n,
        options,
        current,
        [](int) {},
        [this](int selected) {
            if (selected < static_cast<int>(ChiakiLogVerbosity::Normal) ||
                selected > static_cast<int>(ChiakiLogVerbosity::Trace))
                selected = static_cast<int>(ChiakiLogVerbosity::Normal);
            services->developer.setChiakiLogVerbosity(static_cast<ChiakiLogVerbosity>(selected));
            services->store.save();
        });
}

void SettingsDebugView::initDebugDiscoveryLogToggle() {
    bool currentValue = services->developer.discoveryLog();

    debugDiscoveryLogToggle->init(
        "akira/settings/discovery_log"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setDiscoveryLog(isOn);
            services->store.save();
        }
    );
}

void SettingsDebugView::initDebugFfmpegLogToggle() {
    bool currentValue = services->developer.ffmpegLog();

    debugFfmpegLogToggle->init(
        "akira/settings/ffmpeg_log"_i18n,
        currentValue,
        [this](bool isOn) {
            services->developer.setFfmpegLog(isOn);
            services->store.save();
        }
    );
}

}
