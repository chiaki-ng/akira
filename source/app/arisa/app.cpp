#include "app/arisa/app.hpp"

#include <switch.h>

#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour.hpp>

#include <cstdlib>

#include "app/arisa/thread_plan.hpp"
#include "app/crash_report.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"
#include "platform/factory.hpp"

namespace akira::app::arisa {

namespace appearance = ::arisa::appearance;
namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;
namespace theme = ::arisa::theme;

App::App()
    : threading(ThreadPlan::policy())
{
    ServiceGraph::prepareProcess();
    threading.adopt_current_thread(provider::ThreadRole::ui);
}

App::~App() = default;

bool App::openProviders()
{
    CrashReport::stage("arisa providers");
    auto created = provider::horizon::HorizonProviderFactory::create(provider::Providers{ .threading = threading, .audio = silence });
    if (!created)
    {
        log::error("[ARISA] the providers could not start: {}", created.error().explanation());
        return false;
    }
    providers.emplace(std::move(*created));
    auto stock = providers->stock_gpu();
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
    options.appearance = runtime::AppearanceOptions{
        .policy = appearance::AppearancePolicy{
            .application = "akira",
            .defaults = appearance::AppearanceSettings{
                .color_scheme = appearance::ColorSchemePreference::dark,
                .accent = theme::Colour::hex(0x0091FF),
            },
        },
    };
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
    nat.emplace(services.dispatcher, services.core, services.strings);
    CrashReport::stage("arisa stream driver");
    driver = std::make_shared<StreamDriver>(services, *application, *window, model, notices, [this] {
        if (consoleOperations)
            consoleOperations->streamEnded();
    });
    driver->watchFocus();
    CrashReport::stage("arisa discovery");
    consoleOperations.emplace(services, shelf, notices, [this](consoles::Host& host) { driver->start(host); });
    consoleOperations->follow();
    parts = ShellScreen::Parts{
        .shelf = &shelf,
        .consoles = &*consoleOperations,
        .stream = driver.get(),
        .picture = &*picture,
        .controls = &*controls,
        .nat = &*nat,
        .help = &*help,
    };
    CrashReport::stage("arisa shell ready");
    model.ready();
}

bool App::frame()
{
    if (!firstFrameShown)
    {
        firstFrameShown = true;
        CrashReport::stage("arisa first frame");
    }
    window->tick();
    if (driver)
        driver->tick();
    return !appletMainLoop();
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
    nat.reset();
    graph->shutDown();
    nvExit();
    return ran ? EXIT_SUCCESS : EXIT_FAILURE;
}

}
