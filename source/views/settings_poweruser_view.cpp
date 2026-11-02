#include "views/settings_poweruser_view.hpp"
#include "views/benchmark_view.hpp"
#include "views/host_list_tab.hpp"
#include "core/trophy_manager.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <format>

using namespace brls::literals;

SettingsPowerUserView::SettingsPowerUserView() {
    this->inflateFromXMLRes("xml/settings/power_user.xml");

    settings = SettingsManager::getInstance();

    initUnlockBitrateMaxToggle();
    initIpcStatsToggle();
    initPortGuessingToggle();
    initPortGuessingCountSlider();
    initPortGuessingSocksSlider();
    initAutoReconnectToggle();
    initHideAccountNameToggle();

    runBenchmarkBtn->registerClickAction([this](brls::View*) {
        runGhashBenchmark();
        return true;
    });

    flushTrophyCacheBtn->registerClickAction([](brls::View*) {
        TrophyManager::getInstance()->flushCache();
        brls::Application::notify("akira/settings/trophy_cache_flushed"_i18n);
        return true;
    });

}

void SettingsPowerUserView::initUnlockBitrateMaxToggle() {
    unlockBitrateMaxToggle->init(
        "akira/settings/unlock_bitrate_max"_i18n,
        settings->getUnlockBitrateMax(),
        [this](bool isOn) {
            settings->setUnlockBitrateMax(isOn);
            settings->writeFile();
            brls::Logger::info("Unlock Bitrate Max set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initIpcStatsToggle() {
    ipcStatsToggle->init(
        "akira/settings/ipc_stats"_i18n,
        settings->getIpcStatsEnabled(),
        [this](bool isOn) {
            settings->setIpcStatsEnabled(isOn);
            settings->writeFile();
            brls::Logger::info("IPC Stats set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initHideAccountNameToggle() {
    hideAccountNameToggle->init(
        "akira/settings/hide_account_name"_i18n,
        settings->getHideAccountName(),
        [this](bool isOn) {
            settings->setHideAccountName(isOn);
            settings->writeFile();
            HostListTab::notifyAccountNameDisplayChanged();
        }
    );
}

void SettingsPowerUserView::initAutoReconnectToggle() {
    autoReconnectToggle->init(
        "akira/settings/auto_reconnect"_i18n,
        settings->getAutoReconnect(),
        [this](bool isOn) {
            settings->setAutoReconnect(isOn);
            settings->writeFile();
            brls::Logger::info("Auto Reconnect set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initPortGuessingToggle() {
    bool currentValue = settings->getPortGuessing();

    portGuessingToggle->init(
        "akira/settings/port_guessing"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->setPortGuessing(isOn);
            settings->writeFile();
            brls::Logger::info("Port Guessing set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initPortGuessingCountSlider() {
    int current = settings->getPortGuessingCount();
    constexpr int minVal = 1;
    constexpr int maxVal = 75;
    float normalized = static_cast<float>(current - minVal) / (maxVal - minVal);
    normalized = std::max(0.0f, std::min(1.0f, normalized));

    portGuessingCountSlider->detail->setWidth(60);
    portGuessingCountSlider->detail->setShrink(0);
    portGuessingCountSlider->init(
        "akira/settings/port_guess_count"_i18n,
        normalized,
        [this](float value) {
            int count = static_cast<int>(1 + value * 74);
            count = std::max(1, std::min(75, count));
            settings->setPortGuessingCount(count);
            portGuessingCountSlider->detail->setText(std::format("{}", count));
            settings->writeFile();
        }
    );
    portGuessingCountSlider->detail->setText(std::format("{}", current));
    portGuessingCountSlider->slider->setDiscreteStep(1.0f / 74.0f);
}

void SettingsPowerUserView::initPortGuessingSocksSlider() {
    int current = settings->getPortGuessingSocks();
    constexpr int minVal = 1;
    constexpr int maxVal = 250;
    float normalized = static_cast<float>(current - minVal) / (maxVal - minVal);
    normalized = std::max(0.0f, std::min(1.0f, normalized));

    portGuessingSocksSlider->detail->setWidth(60);
    portGuessingSocksSlider->detail->setShrink(0);
    portGuessingSocksSlider->init(
        "akira/settings/probe_socket_count"_i18n,
        normalized,
        [this](float value) {
            int count = static_cast<int>(1 + value * 249);
            count = std::max(1, std::min(250, count));
            settings->setPortGuessingSocks(count);
            portGuessingSocksSlider->detail->setText(std::format("{}", count));
            settings->writeFile();
        }
    );
    portGuessingSocksSlider->detail->setText(std::format("{}", current));
    portGuessingSocksSlider->slider->setDiscreteStep(1.0f / 249.0f);
}

void SettingsPowerUserView::runGhashBenchmark() {
    auto* benchmarkView = new BenchmarkView();
    brls::Application::pushActivity(new brls::Activity(benchmarkView));
    benchmarkView->startBenchmark();
}
