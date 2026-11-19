#include "settings/views/settings_poweruser_view.hpp"
#include "app/view_services.hpp"
#include "app/views/benchmark_view.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "trophies/trophy_manager.hpp"
#include "platform/factory.hpp"
#include "log/log.hpp"

#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <format>

namespace akira::settings::views {

using namespace brls::literals;

SettingsPowerUserView::SettingsPowerUserView() {
    this->inflateFromXMLRes("xml/settings/power_user.xml");

    settings = SettingsManager::getInstance();

    initUnlockBitrateMaxToggle();
    if (auto& platform = app::ViewServices::current().platform; platform.supplies(akira::platform::PlatformCapability::stats_export))
        platform.stats().initToggle(*ipcStatsToggle);
    else
        ipcStatsToggle->setVisibility(brls::Visibility::GONE);
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
        app::ViewServices::current().trophies.flushCache();
        brls::Application::notify("akira/settings/trophy_cache_flushed"_i18n);
        return true;
    });

}

void SettingsPowerUserView::initUnlockBitrateMaxToggle() {
    unlockBitrateMaxToggle->init(
        "akira/settings/unlock_bitrate_max"_i18n,
        settings->stream().unlockBitrateMax(),
        [this](bool isOn) {
            settings->stream().setUnlockBitrateMax(isOn);
            settings->store().save();
            log::info("Unlock Bitrate Max set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initHideAccountNameToggle() {
    hideAccountNameToggle->init(
        "akira/settings/hide_account_name"_i18n,
        settings->app().hideAccountName(),
        [this](bool isOn) {
            settings->app().setHideAccountName(isOn);
            settings->store().save();
            consoles::views::HostListTab::notifyAccountNameDisplayChanged();
        }
    );
}

void SettingsPowerUserView::initAutoReconnectToggle() {
    autoReconnectToggle->init(
        "akira/settings/auto_reconnect"_i18n,
        settings->network().autoReconnect(),
        [this](bool isOn) {
            settings->network().setAutoReconnect(isOn);
            settings->store().save();
            log::info("Auto Reconnect set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initPortGuessingToggle() {
    bool currentValue = settings->network().portGuessing();

    portGuessingToggle->init(
        "akira/settings/port_guessing"_i18n,
        currentValue,
        [this](bool isOn) {
            settings->network().setPortGuessing(isOn);
            settings->store().save();
            log::info("Port Guessing set to {}", isOn ? "true" : "false");
        }
    );
}

void SettingsPowerUserView::initPortGuessingCountSlider() {
    int current = settings->network().portGuessingCount();
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
            settings->network().setPortGuessingCount(count);
            portGuessingCountSlider->detail->setText(std::format("{}", count));
            settings->store().save();
        }
    );
    portGuessingCountSlider->detail->setText(std::format("{}", current));
    portGuessingCountSlider->slider->setDiscreteStep(1.0f / 74.0f);
}

void SettingsPowerUserView::initPortGuessingSocksSlider() {
    int current = settings->network().portGuessingSocks();
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
            settings->network().setPortGuessingSocks(count);
            portGuessingSocksSlider->detail->setText(std::format("{}", count));
            settings->store().save();
        }
    );
    portGuessingSocksSlider->detail->setText(std::format("{}", current));
    portGuessingSocksSlider->slider->setDiscreteStep(1.0f / 249.0f);
}

void SettingsPowerUserView::runGhashBenchmark() {
    auto* benchmarkView = new app::views::BenchmarkView();
    brls::Application::pushActivity(new brls::Activity(benchmarkView));
    benchmarkView->startBenchmark();
}

}
