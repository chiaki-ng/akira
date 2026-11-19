#include "app/app.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <SDL2/SDL.h>
#include <curl/curl.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

extern "C" {
#include <libavutil/log.h>
}

#include "accounts/views/psn_gated_box.hpp"
#include "akira_version.h"
#include "app/crash_report.hpp"
#include "app/views/applet_warning_activity.hpp"
#include "app/views/build_info_tab.hpp"
#include "app/views/main_activity.hpp"
#include "app/views/theme.hpp"
#include "app/views/vendored/switchfin/recycling_grid.hpp"
#include "app/worker_cores.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/views/add_host_tab.hpp"
#include "consoles/views/enter_pin_view.hpp"
#include "consoles/views/host_list_tab.hpp"
#include "log/app_log.hpp"
#include "log/log.hpp"
#include "log/log_files.hpp"
#include "network/views/dns_warning_dialog.hpp"
#include "network/views/network_utilities_tab.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/views/stream_view.hpp"
#include "settings/settings_manager.hpp"
#include "settings/views/config_view_tab.hpp"
#include "trophies/views/trophy_list_tab.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"

namespace akira::app {

App::App(int argc, char* argv[])
    : argc(argc), argv(argv), mainPlaced(prepareProcess())
{
}

Result App::prepareProcess()
{
    CrashReport::install("akira", AKIRA_VERSION_STRING, "/switch/akira/logs");
    if (std::freopen("sdmc:/switch/akira/logs/akira-stderr.txt", "w", stderr))
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    log::Log::process().setLevel(log::Level::info);
    return WorkerCores::placeMainThread();
}

void App::forwardFfmpegLog(void*, int level, const char* format, va_list arguments)
{
    if (settings::SettingsManager::getInstance()->developer().silenceAllLogs() || !settings::SettingsManager::getInstance()->developer().ffmpegLog())
        return;
    if (level > av_log_get_level())
        return;
    std::array<char, 512> buf{};
    vsnprintf(buf.data(), buf.size(), format, arguments);
    std::string_view msg(buf.data());
    if (msg.ends_with('\n'))
        msg.remove_suffix(1);
    if (msg.empty())
        return;
    if (level <= AV_LOG_ERROR)
        log::error("ffmpeg: {}", msg);
    else if (level <= AV_LOG_WARNING)
        log::warning("ffmpeg: {}", msg);
    else
        log::info("ffmpeg: {}", msg);
}

const char* App::appletTypeName(AppletType type)
{
    switch (type)
    {
        case AppletType_None:              return "None";
        case AppletType_Default:           return "Default";
        case AppletType_Application:       return "Application";
        case AppletType_SystemApplet:      return "SystemApplet";
        case AppletType_LibraryApplet:     return "LibraryApplet";
        case AppletType_OverlayApplet:     return "OverlayApplet";
        case AppletType_SystemApplication: return "SystemApplication";
        default:                           return "Unknown";
    }
}

void App::openLog()
{
    auto* settings = settings::SettingsManager::getInstance();
    settings->developer().applyChiakiLogVerbosity();
    if (settings->developer().fileLogging())
        log::AppLog::open(log::LogFiles::appLogPath());
    CrashReport::stage("log open");
    log::info("akira {} starting", AKIRA_VERSION_STRING);
    log::info("main thread core {} placement {:#x}", svcGetCurrentProcessorNumber(), mainPlaced);
}

void App::readArguments()
{
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "-d") == 0)
        {
            log::Log::process().setLevel(log::Level::debug);
        }
        else if (std::strcmp(argv[i], "-v") == 0)
        {
            brls::Application::enableDebuggingView(true);
        }
    }
}

void App::prepareLocale()
{
    std::string overrideLocale = settings::SettingsManager::getInstance()->app().debugLocale();
    if (!overrideLocale.empty()) {
        brls::Platform::APP_LOCALE_DEFAULT = overrideLocale;
    } else {
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    }
}

bool App::startFrameworks()
{
    CrashReport::stage("borealis init");
    if (!brls::Application::init())
    {
        log::error("Unable to init Borealis application");
        return false;
    }

    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 369.0f);
    brls::getStyle().addMetric("brls/sidebar/item_font_size", 24.0f);

    brls::Application::getWindowFocusChangedEvent()->subscribe([](bool focused) {
        if (focused)
            httpMarkConnectionsStale();
    });

    CrashReport::stage("sdl init");
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        log::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    CrashReport::stage("curl init");
    const CURLcode curlInit = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curlInit != CURLE_OK)
    {
        log::error("[NET] curl_global_init failed code={} reason=\"{}\"",
            static_cast<int>(curlInit), curl_easy_strerror(curlInit));
        return false;
    }
    return true;
}

std::function<void()> App::signInRenewed()
{
#if AKIRA_CLOUD
    return [this] { cloud->refreshInBackground(); };
#else
    return {};
#endif
}

void App::startCore()
{
    CrashReport::stage("core init");
    core.emplace(*settings::SettingsManager::getInstance(), signInRenewed());
    CrashReport::stage("post core");
    if (!core->opened())
        log::error("[CORE] running without the core: streaming, sign-in and discovery are unavailable");
    brls::Application::getWindowFocusChangedEvent()->subscribe([this](bool focused) {
        if (focused && core)
            core->http().drop_connections("applet regained focus");
    });
}

void App::startAsyncLogging()
{
    log::Log::process().setAsync(true);
    if (const curl_version_info_data* curlInfo = curl_version_info(CURLVERSION_NOW))
    {
        log::info(
            "[NET] curl_runtime version={} ssl={} features=0x{:x} ipv6={} async_dns={}",
            curlInfo->version ? curlInfo->version : "-",
            curlInfo->ssl_version ? curlInfo->ssl_version : "-",
            static_cast<unsigned long long>(curlInfo->features),
            (curlInfo->features & CURL_VERSION_IPV6) != 0,
            (curlInfo->features & CURL_VERSION_ASYNCHDNS) != 0);
    }
    brls::Application::getRunLoopEvent()->subscribe([]() {
        using namespace std::chrono_literals;
        static auto nextFlush = std::chrono::steady_clock::now();
        static std::atomic<bool> flushInFlight{false};
        const auto now = std::chrono::steady_clock::now();
        if (now < nextFlush || flushInFlight.exchange(true, std::memory_order_acq_rel))
            return;
        nextFlush = now + 250ms;

        brls::async([]() {
            log::Log::process().flush();
            flushInFlight.store(false, std::memory_order_release);
        }, true);
    });
    log::info("Async logging enabled via thread pool");

    log::info("Chiaki library initialized");
}

void App::registerViews()
{
    brls::Application::registerXMLView("HostListTab", consoles::views::HostListTab::create);
    brls::Application::registerXMLView("PsnGatedBox", accounts::views::PsnGatedBox::create);
    brls::Application::registerXMLView("RecyclingGrid", views::RecyclingGrid::create);
    brls::Application::registerXMLView("TrophyListTab", trophies::views::TrophyListTab::create);
    brls::Application::registerXMLView("AddHostTab", consoles::views::AddHostTab::create);
    brls::Application::registerXMLView("BuildInfoTab", views::BuildInfoTab::create);
    brls::Application::registerXMLView("ConfigViewTab", settings::views::ConfigViewTab::create);
    brls::Application::registerXMLView("NetworkUtilitiesTab", network::views::NetworkUtilitiesTab::create);
    brls::Application::registerXMLView("StreamView", play::views::StreamView::create);
    brls::Application::registerXMLView("EnterPinView", consoles::views::EnterPinView::create);
}

void App::openWindow()
{
    CrashReport::stage("create window");
    brls::Application::createWindow("Akira");

    if (!brls::Application::loadFontFromFile("mono", BRLS_ASSET("font/Cousine-Regular.ttf")))
        log::warning("Could not load mono font, stats overlay will fall back to the regular font");

    brls::Application::getPlatform()->exitToHomeMode(false);
}

void App::runMainLoop()
{
    CrashReport::stage("main loop");
    try{
        while (brls::Application::mainLoop())
        {
        }
    } catch (const std::exception& e) {
        log::error("CRASH: {}", e.what());
        fflush(stdout);
    }
}

void App::startServices()
{
    CrashReport::stage("services");
    wireGuard.emplace(settings::SettingsManager::getInstance()->developer());
    trophies.emplace(HttpPool::instance());
#if AKIRA_CLOUD
    cloud.emplace(settings::SettingsManager::getInstance()->accounts(), HttpPool::instance());
#endif
    auto* settings = settings::SettingsManager::getInstance();
    factory = platform::PlatformFactory::create(platform::PlatformServices{
        .stream = settings->stream(),
        .streamState = settings->streamState(),
        .controllers = settings->controllers(),
        .developer = settings->developer(),
        .app = settings->app(),
        .accounts = settings->accounts(),
        .store = settings->store(),
        .wireGuard = *wireGuard,
        .http = HttpPool::instance(),
        .trophies = *trophies,
    });
    discovery.emplace(settings->hosts(), settings->store(), settings->network(), settings->developer(),
        settings->accounts(), *wireGuard);
    preferences.emplace(settings->stream(), settings->cloud(), settings->network(), settings->developer(),
        *factory, *wireGuard);
    session.emplace(*factory, settings->streamState(), settings->controllers(),
        settings->developer());
    services.emplace(Services{
        .platform = *factory,
        .wireGuard = *wireGuard,
        .discovery = *discovery,
        .preferences = *preferences,
        .session = *session,
        .trophies = *trophies,
#if AKIRA_CLOUD
        .cloud = *cloud,
#endif
    });
    viewServices.emplace(*services);
}

void App::shutDown()
{
    log::info("Application exiting");

    discovery->setServiceEnabled(false);
    for (auto& [name, host] : settings::SettingsManager::getInstance()->hosts().entries())
    {
        if (host)
        {
            host->abandonRegistration();
            host->releaseConsole();
        }
    }

    HttpPool::instance().stop();

    wireGuard->disconnect();

    core.reset();

    session->abandon();

    SDL_Quit();
    curl_global_cleanup();

    // Drain the final batch after all subsystems have reported shutdown.
    log::Log::process().flush();
    nvExit();
}

int App::run()
{
    CrashReport::stage("settings");
    settings::SettingsManager::getInstance();
    openLog();
    readArguments();

    av_log_set_callback(forwardFfmpegLog);

    prepareLocale();

    CrashReport::stage("theme");
    views::setActiveTheme(settings::SettingsManager::getInstance()->app().uiTheme());
    views::applyToBorealis();

    startServices();

    factory->updates().prepare(argc, argv);
    factory->display().prepare();

    if (!startFrameworks())
        return EXIT_FAILURE;

    startCore();
    startAsyncLogging();

    AppletType appletType = appletGetAppletType();
    log::info("Applet type: {} ({})", appletTypeName(appletType), static_cast<int>(appletType));

    registerViews();

    factory->prepareVideo();

    openWindow();

    if (appletType == AppletType_Application)
    {
        brls::Application::pushActivity(new views::MainActivity());
        network::views::DnsWarningDialog::checkAtStartup();
    }
    else
    {
        brls::Application::pushActivity(new views::AppletWarningActivity());
    }

    runMainLoop();
    shutDown();
    return EXIT_SUCCESS;
}

}
