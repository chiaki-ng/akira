#include "app/service_graph.hpp"

#include <condition_variable>
#include <mutex>

#include <SDL2/SDL.h>
#include <curl/curl.h>

#include <chrono>
#include <cstdio>

#include "akira_version.h"
#include "app/crash_report.hpp"
#include "log/app_log.hpp"
#include "log/log.hpp"
#include "log/log_files.hpp"
#include "util/http.hpp"

namespace akira::app {

ServiceGraph::ServiceGraph(UiSeams seams)
    : seams(seams)
    , resolverHook(dns)
    , store((CrashReport::stage("settings"), SettingsDirectory))
    , developer_(store)
    , stream(store, fsr4k())
    , controllers(store)
    , cloudSettings(store)
    , networkSettings(store, [this](const std::vector<network::Ipv4Address>& servers) { dns.configure(servers); })
    , accountSettings(store, [](bool legacy) { httpSetSonyBlocked(legacy); })
    , consoleSettings(store)
    , appSettings_(store)
    , streamState(stream, cloudSettings, developer_)
{
    store.finishLoading();
}

ServiceGraph::~ServiceGraph() = default;

bool ServiceGraph::fsr4k()
{
    return (platform::PlatformFactory::fixedCapabilities() & platform::PlatformCapability::fsr_4k)
        == platform::PlatformCapability::fsr_4k;
}

void ServiceGraph::prepareProcess()
{
    CrashReport::install("akira", AKIRA_VERSION_STRING, "/switch/akira/logs");
    if (std::freopen("sdmc:/switch/akira/logs/akira-stderr.txt", "w", stderr))
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    log::Log::process().setLevel(log::Level::info);
}

void ServiceGraph::openLog()
{
    developer_.applyChiakiLogVerbosity();
    if (developer_.fileLogging())
        log::AppLog::open(log::LogFiles::appLogPath());
    CrashReport::stage("log open");
    log::info("akira {} starting", AKIRA_VERSION_STRING);
}

void ServiceGraph::startFfmpegLog()
{
    ffmpegLog.emplace(developer_);
}

bool ServiceGraph::startCurl()
{
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

std::function<void()> ServiceGraph::signInRenewed()
{
#if AKIRA_CLOUD
    return [this] { cloud->refreshInBackground(); };
#else
    return {};
#endif
}

void ServiceGraph::startCore()
{
    CrashReport::stage("core init");
    core.emplace(play::ChiakiCore::Settings{
                     .store = store,
                     .developer = developer_,
                     .cloud = cloudSettings,
                     .accounts = accountSettings,
                     .consoles = consoleSettings,
                 },
        seams.dispatcher, signInRenewed());
    CrashReport::stage("post core");
    if (!core->opened())
        log::error("[CORE] running without the core: streaming, sign-in and discovery are unavailable");
}

void ServiceGraph::startServices()
{
    CrashReport::stage("services");
    hosts.emplace(consoleSettings, accountSettings, appSettings_, store, seams.dispatcher, *core);
    http.emplace(core->threading());
    wireGuard.emplace(developer_, *core);
    art.emplace(*http, seams.dispatcher);
    trophies.emplace(*http, *art, seams.dispatcher, *core);
#if AKIRA_CLOUD
    cloud.emplace(accountSettings, *http, seams.dispatcher, seams.strings, *core);
#endif
    factory = platform::PlatformFactory::create(platform::PlatformServices{
        .stream = stream,
        .streamState = streamState,
        .controllers = controllers,
        .developer = developer_,
        .app = appSettings_,
        .wireGuard = *wireGuard,
        .threading = core->threading(),
        .gpu = seams.gpu,
        .rumble = seams.rumble,
        .overlayPalette = seams.overlayPalette,
    });
    discovery.emplace(*hosts, store, networkSettings, developer_, accountSettings, *wireGuard, seams.dispatcher,
        seams.window, *core);
    preferences.emplace(stream, cloudSettings, networkSettings, developer_, *factory, *wireGuard);
    session.emplace(*factory, streamState, controllers, developer_, seams.window, core->threading(), seams.audio);
    services_.emplace(Services{
        .dispatcher = seams.dispatcher,
        .window = seams.window,
        .strings = seams.strings,
        .store = store,
        .developer = developer_,
        .stream = stream,
        .controllers = controllers,
        .cloudSettings = cloudSettings,
        .networkSettings = networkSettings,
        .accounts = accountSettings,
        .consoleSettings = consoleSettings,
        .appSettings = appSettings_,
        .hosts = *hosts,
        .streamState = streamState,
        .dns = dns,
        .http = *http,
        .platform = *factory,
        .wireGuard = *wireGuard,
        .discovery = *discovery,
        .discoveryGate = discoveryGate,
        .registration = registration,
        .preferences = *preferences,
        .session = *session,
        .trophies = *trophies,
        .art = *art,
        .core = *core,
#if AKIRA_CLOUD
        .cloud = *cloud,
#endif
    });
}

bool ServiceGraph::startSdl(bool audio)
{
    CrashReport::stage("sdl init");
    if (SDL_Init((audio ? SDL_INIT_AUDIO : 0u) | SDL_INIT_JOYSTICK) < 0)
    {
        log::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

void ServiceGraph::followFocus()
{
    focusWatches.push_back(seams.window.watchFocus([](bool focused) {
        if (focused)
            httpMarkConnectionsStale();
    }));
    focusWatches.push_back(seams.window.watchFocus([this](bool focused) {
        if (focused && core)
            core->http().drop_connections("applet regained focus");
    }));
}

void ServiceGraph::startAsyncLogging()
{
    using namespace std::chrono_literals;
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
    logFlush = std::jthread([](std::stop_token stop) {
        std::mutex idle;
        std::condition_variable_any wake;
        std::unique_lock lock(idle);
        while (!stop.stop_requested())
        {
            wake.wait_for(lock, stop, 250ms, [&stop] { return stop.stop_requested(); });
            log::Log::process().flush();
        }
    });
    log::info("Async logging enabled via thread pool");

    log::info("Chiaki library initialized");
}

void ServiceGraph::shutDown()
{
    log::info("Application exiting");

    logFlush = {};
    focusWatches.clear();

    discovery->setServiceEnabled(false);
    for (auto& [name, host] : hosts->entries())
    {
        if (host)
        {
            host->abandonRegistration();
            host->releaseConsole();
        }
    }

    http->stop();

    wireGuard->disconnect();

    session->abandon();

    core.reset();

    SDL_Quit();
    curl_global_cleanup();

    log::Log::process().flush();
}

}
