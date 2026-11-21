#include "app/arisa/app.hpp"

#include <switch.h>

#include <arisa/runtime/application_options.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <utility>

#include "app/arisa/thread_plan.hpp"
#include "app/build_info.hpp"
#include "app/crash_report.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"
#include "log/log_files.hpp"
#include "network/nat_probe.hpp"
#include "network/nat_report.hpp"
#include "platform/factory.hpp"
#include "platform/video/provider.hpp"
#include "platform/video/renderer_choice.hpp"
#include "play/couch_seats.hpp"
#include "settings/settings_store.hpp"
#include "settings/takion_target.hpp"

#include <libchiaking/play_types.hpp>

#include <ranges>
#include <vector>

namespace akira::app::arisa {

namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

App::App()
    : threading(ThreadPlan::policy())
{
    ServiceGraph::prepareProcess();
    installCrashReporter();
    threading.adopt_current_thread(provider::ThreadRole::ui);
}

App::~App() = default;

void App::installCrashReporter()
{
    CrashReport::stage("arisa crash reporter");
    auto installed = provider::CrashReporter::install({ .directory = crashDirectory, .app = crashApp });
    if (!installed)
    {
        log::error("[ARISA] the crash reporter could not install under {}: error {}", crashDirectory,
            std::to_underlying(installed.error()));
        return;
    }
    crashes.emplace(std::move(*installed));
}

bool App::openProviders()
{
    CrashReport::stage("arisa providers");
    auto created = provider::horizon::HorizonProviderFactory::create(provider::Providers{ .threading = threading }, {},
        Gpu::options());
    if (!created)
    {
        log::error("[ARISA] the providers could not start: {}", created.error().explanation());
        return false;
    }
    providers.emplace(std::move(*created));
    auto stock = providers->stock_gpu<Gpu::Stock>();
    if (!stock)
    {
        log::error("[ARISA] the console GPU is unavailable: {}", providers->stock_gpu_report().value_or("no report"));
        return false;
    }
    window.emplace(providers->surface(), providers->lifecycle());
    strings.emplace(providers->assets(), StringCatalog::fallbackLocale);
    if (!strings->loaded())
        log::warning("[ARISA] no string catalog under romfs:/{}", StringCatalog::directory);
    gpu.emplace(stock->get(), providers->surface());
    rumble.emplace(providers->haptics());
    voice.emplace(providers->audio());
    return true;
}

bool App::openApplication()
{
    CrashReport::stage("arisa application");
    runtime::ApplicationOptions options{};
    options.appearance = themes.options();
    options.appearance->storage = appearanceStore;
    auto created = runtime::Application::create(*providers, ShellScreen(model, notices, parts, *strings), std::move(options));
    if (!created)
    {
        log::error("[ARISA] the application could not start: {}", created.error().explanation());
        return false;
    }
    application.emplace(std::move(*created));
    stage.emplace(*application, *providers, gpu->handoff(), notices);
    dispatcher.emplace(application->root().dispatcher(), application->root().scheduler(),
        application->executors().background.get());
    graph.emplace(UiSeams{
        .dispatcher = *dispatcher,
        .window = *window,
        .strings = *strings,
        .gpu = *gpu,
        .rumble = *rumble,
        .audio = *voice,
        .overlayPalette = overlayPalette,
    });
    return true;
}

void App::startServices()
{
    CrashReport::stage("arisa log");
    graph->openLog();
    log::info("[ARISA] ui thread core {}", svcGetCurrentProcessorNumber());
    CrashReport::stage("arisa ffmpeg log");
    graph->startFfmpegLog();
    CrashReport::stage("arisa core");
    graph->startCore();
    CrashReport::stage("arisa services");
    graph->startServices();
    CrashReport::stage("arisa display");
    graph->platform().display().prepare();
    CrashReport::stage("arisa sdl");
    static_cast<void>(graph->startSdl(false));
    CrashReport::stage("arisa focus");
    graph->followFocus();
    CrashReport::stage("arisa async log");
    graph->startAsyncLogging();
    CrashReport::stage("arisa prepare video");
    graph->platform().prepareVideo();
    buildScreens();
}

void App::buildScreens()
{
    Services& services = graph->services();
    CrashReport::stage("arisa settings screens");
    help.emplace(helpDirectory, std::string());
    log::info("[ARISA] {} help entries", help->size());
    picture.emplace(services.stream, services.store, services.platform.supplies(platform::PlatformCapability::fsr_4k));
    sound.emplace(services.stream, services.store);
    controls.emplace(services.controllers, services.store);
    nat.emplace(services.dispatcher, [&core = services.core] { return network::NatReport(network::NatProbe(core).check()); },
        services.strings);
    upkeep.emplace(services,
        [this] {
            if (consoleOperations)
                consoleOperations->refresh();
        },
        [this] { quitting = true; });
    padBench.emplace(services, *rumble);
    tunnel.emplace(services);
    tunnelControl.emplace(services.dispatcher, *tunnel);
    capture.emplace(*stage);
    settingsModel.emplace(settings::ui::SettingsModel::Parts{
        .store = services.store,
        .developer = services.developer,
        .stream = services.stream,
        .controllers = services.controllers,
        .network = services.networkSettings,
        .app = services.appSettings,
        .cloud = services.cloudSettings,
        .maintenance = *upkeep,
        .pads = *padBench,
        .capture = *capture,
        .notifier = notices,
        .features = features(),
    });
    mapping.emplace(services.controllers, services.store, *controls, *capture);
    CrashReport::stage("arisa accounts");
    desk.emplace(services, notices, [this] { profileChanged(); });
    accountModel.emplace(accounts::ui::AccountModel::Parts{
        .desk = *desk,
        .notifier = notices,
        .strings = services.strings,
        .emptied = [this] { model.showSettings(false); },
    });
    logs.emplace(log::LogFiles::directory);
    CrashReport::stage("arisa couch");
    buildCouch();
    CrashReport::stage("arisa stream driver");
    driver = std::make_shared<StreamDriver>(services, *stage, *window, model, notices, *controls, *picture, *sound, *mapping,
        StreamDriver::Couch{ .table = couchBridge ? &*couchBridge : nullptr, .model = couch ? &*couch : nullptr }, [this] {
            if (consoleOperations)
                consoleOperations->streamEnded();
        });
    driver->watchFocus();
    if (couchBridge && couch)
        couchBridge->follow(CouchBridge::Events{
            .joined = [this](play::PadId pad, std::uint8_t slot) { couch->joined(pad, slot); },
            .arrived = [this](play::PadId pad) {
                if (driver)
                    driver->padArrived(pad);
            },
        });
    CrashReport::stage("arisa discovery");
    consoleOperations.emplace(services, shelf, recent, notices, [this](consoles::Host& host) { driver->start(host); },
        [this] {
            if (accountModel)
                accountModel->welcome(false, accountModel->view().get().entries.empty());
        });
    CrashReport::stage("arisa pages");
    buildPages();
    parts = ShellScreen::Parts{
        .shelf = &shelf,
        .consoles = &*consoleOperations,
        .stream = driver.get(),
        .settings = settings::ui::SettingsScreen::Parts{
            .picture = &*picture,
            .model = &*settingsModel,
            .mapping = &*mapping,
            .nat = &*nat,
            .tunnel = &*tunnelControl,
            .help = &*help,
            .accounts = &*accountModel,
            .logs = &*logs,
        },
        .art = &*artSession,
        .recent = &recent,
        .trophies = &*trophyModel,
#if AKIRA_CLOUD
        .cloud = &*cloudModel,
#endif
    };
    offerPages();
    CrashReport::stage("arisa shell ready");
    accountModel->reload();
    model.ready();
    accountModel->openIfEmpty();
}

void App::buildPages()
{
    Services& services = graph->services();
    art.emplace(services.http, services.dispatcher);
    artSession.emplace(*art);
    trophyDesk.emplace(services);
    trophyModel.emplace(*trophyDesk, notices, services.strings);
#if AKIRA_CLOUD
    cloudDesk.emplace(
        services,
        [this](std::shared_ptr<consoles::Host> host) {
            if (driver)
                driver->startCloud(std::move(host));
        },
        [this] {
            if (accountModel)
                accountModel->startQr();
        });
    cloudModel.emplace(*cloudDesk, notices, services.strings);
#endif
}

void App::profileChanged()
{
    if (consoleOperations)
        consoleOperations->refresh();
    if (trophyModel)
        trophyModel->reset();
    offerPages();
}

void App::offerPages()
{
#if AKIRA_CLOUD
    constexpr bool cloud = true;
#else
    constexpr bool cloud = false;
#endif
    model.offer(cloud, graph->services().accounts.activeProfileTrophiesEnabled());
}

void App::buildCouch()
{
    Services& services = graph->services();
    if (!services.platform.supplies(platform::PlatformCapability::couch_seats))
        return;
    play::CouchSeats* seats = services.platform.couchSeats();
    if (!seats)
        return;
    couchBridge.emplace(*seats, services.accounts, services.appSettings, services.dispatcher);
    couch.emplace(*couchBridge, services.strings);
    log::info("[ARISA] couch seats available");
}

auto App::features() -> settings::DeviceFeatures
{
    using libchiaking::play::DeveloperTuning;
    Services& services = graph->services();
    platform::PlatformFactory& platform = services.platform;
    settings::DeviceFeatures made{
        .selfUpdate = platform.supplies(platform::PlatformCapability::self_update),
        .statsExport = platform.supplies(platform::PlatformCapability::stats_export),
        .fsr4k = platform.supplies(platform::PlatformCapability::fsr_4k),
        .hevc = platform.supplies(platform::PlatformCapability::hevc),
        .accounts = true,
        .configFile = services.store.path(),
        .buildInfoFile = std::filesystem::path(BuildInfo::file),
        .logFolder = std::filesystem::path(log::LogFiles::directory),
        .takionPs4 = DeveloperTuning::implemented_protocol_versions(libchiaking::consoles::Generation::ps4),
        .takionPs5 = DeveloperTuning::implemented_protocol_versions(libchiaking::consoles::Generation::ps5),
    };
#if AKIRA_CLOUD
    made.cloud = true;
    made.takionPs5Cloud = DeveloperTuning::implemented_protocol_versions(libchiaking::catalog::ServiceGeneration::ps5_cloud)
        | std::views::filter([](std::uint32_t version) { return version <= settings::highestCloudTakion; })
        | std::ranges::to<std::vector>();
    made.takionPsNow = DeveloperTuning::implemented_protocol_versions(libchiaking::catalog::ServiceGeneration::old_service);
#endif
    for (const platform::RendererChoice& choice : platform.rendererChoices())
        made.renderers.push_back({ choice.label, choice.kind });
    const std::unique_ptr<platform::VideoProvider> video = platform.createVideo();
    const platform::VideoOptions options = video->options();
    made.softwareHevc = options.softwareHevc;
    made.zeroCopy = options.zeroCopy;
    made.videoName = video->name();
    return made;
}

bool App::frame()
{
    if (!firstFrameShown)
    {
        firstFrameShown = true;
        CrashReport::stage("arisa first frame");
        consoleOperations->follow();
        if (settingsModel && settingsModel->features().selfUpdate)
            upkeep->checkOnLaunch(notices);
    }
    window->tick();
    if (screenshots)
        screenshots->tick();
    if (driver)
        driver->tick();
    const auto now = std::chrono::steady_clock::now();
    if (trophyModel)
        trophyModel->tick(now);
#if AKIRA_CLOUD
    if (cloudModel)
        cloudModel->tick(now);
#endif
    return !quitting && !model.view().get().exitConfirmed && !appletMainLoop();
}

int App::run()
{
    if (!openProviders() || !openApplication())
        return EXIT_FAILURE;

    if (!graph->startCurl())
        return EXIT_FAILURE;
    startServices();
    screenshots.emplace(providers->gpu(), *gpu, notices, graph->services().appSettings, *strings, *dispatcher);
    screenshotWatch.emplace(application->observe(*screenshots));

    CrashReport::stage("arisa main loop");
    const auto ran = application->run_until([this](runtime::Root&) { return frame(); });
    if (!ran)
        log::error("[ARISA] the frame loop stopped: {}", ran.error().explanation());

#if AKIRA_CLOUD
    cloudModel.reset();
    cloudDesk.reset();
#endif
    screenshotWatch.reset();
    screenshots.reset();
    trophyModel.reset();
    trophyDesk.reset();
    consoleOperations.reset();
    driver.reset();
    couch.reset();
    couchBridge.reset();
    accountModel.reset();
    desk.reset();
    logs.reset();
    mapping.reset();
    settingsModel.reset();
    tunnelControl.reset();
    nat.reset();
    graph->shutDown();
    return ran ? EXIT_SUCCESS : EXIT_FAILURE;
}

}
