#include "app/arisa/app.hpp"

#include <switch.h>

#include <arisa/runtime/application_options.hpp>

#include <cstdlib>
#include <utility>

#include "app/arisa/thread_plan.hpp"
#include "app/crash_report.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"
#include "network/nat_probe.hpp"
#include "network/nat_report.hpp"
#include "platform/factory.hpp"
#include "platform/video/provider.hpp"
#include "platform/video/renderer_choice.hpp"
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
    auto created = provider::horizon::HorizonProviderFactory::create(
        provider::Providers{ .threading = threading, .audio = silence }, {}, provider::horizon::GpuOptions{ .api = Gpu::Api });
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
    dispatcher.emplace(application->root().dispatcher(), application->root().scheduler(),
        application->executors().background.get());
    graph.emplace(UiSeams{
        .dispatcher = *dispatcher,
        .window = *window,
        .strings = *strings,
        .gpu = *gpu,
        .rumble = rumble,
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
    static_cast<void>(graph->startSdl());
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
    controls.emplace(services.controllers, services.store);
    nat.emplace(services.dispatcher, [&core = services.core] { return network::NatReport(network::NatProbe(core).check()); },
        services.strings);
    upkeep.emplace(services,
        [this] {
            if (consoleOperations)
                consoleOperations->refresh();
        },
        [this] { quitting = true; });
    padBench.emplace(services, rumble);
    tunnel.emplace(services);
    tunnelControl.emplace(services.dispatcher, *tunnel);
    capture.emplace(*application);
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
    CrashReport::stage("arisa stream driver");
    driver = std::make_shared<StreamDriver>(services, *application, *window, model, notices, *controls, *mapping, [this] {
        if (consoleOperations)
            consoleOperations->streamEnded();
    });
    driver->watchFocus();
    CrashReport::stage("arisa discovery");
    consoleOperations.emplace(services, shelf, notices, [this](consoles::Host& host) { driver->start(host); });
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
        },
    };
    CrashReport::stage("arisa shell ready");
    model.ready();
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
        .configFile = services.store.path(),
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
    if (driver)
        driver->tick();
    return !quitting && !appletMainLoop();
}

int App::run()
{
    if (!openProviders() || !openApplication())
        return EXIT_FAILURE;

    if (!graph->startCurl())
        return EXIT_FAILURE;
    startServices();

    CrashReport::stage("arisa main loop");
    const auto ran = application->run_until([this](runtime::Root&) { return frame(); });
    if (!ran)
        log::error("[ARISA] the frame loop stopped: {}", ran.error().explanation());

    consoleOperations.reset();
    driver.reset();
    mapping.reset();
    settingsModel.reset();
    tunnelControl.reset();
    nat.reset();
    graph->shutDown();
    return ran ? EXIT_SUCCESS : EXIT_FAILURE;
}

}
