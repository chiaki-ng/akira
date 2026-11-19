#include "app/arisa/app.hpp"

#include <switch.h>

#include <cstdlib>

#include "app/arisa/shell_screen.hpp"
#include "app/arisa/thread_plan.hpp"
#include "app/crash_report.hpp"
#include "consoles/host.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;
namespace runtime = ::arisa::runtime;

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
    auto created = provider::horizon::HorizonProviderFactory::create(provider::Providers{ .threading = threading });
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
    auto created = runtime::Application::create(*providers, ShellScreen(model, *this, *strings),
        runtime::ApplicationOptions{});
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
    graph->openLog();
    log::info("[ARISA] ui thread core {}", svcGetCurrentProcessorNumber());
    graph->startFfmpegLog();
    graph->startCore();
    graph->startServices();
    graph->platform().display().prepare();
    static_cast<void>(graph->startSdl());
    graph->followFocus();
    graph->startAsyncLogging();
    graph->platform().prepareVideo();
    driver = std::make_shared<StreamDriver>(graph->services(), *application, *window, model);
    followConsoles();
}

void App::followConsoles()
{
    Services& services = graph->services();
    services.discovery.setOnHostsChanged([this] {
        dispatcher->post([this] { model.list(graph->services().hosts.facts()); });
    });
    services.discovery.setServiceEnabled(true);
    model.list(services.hosts.facts());
}

void App::connect(const std::string& catalogName)
{
    if (!graph || !driver)
        return;
    auto& hosts = graph->services().hosts.entries();
    const auto found = hosts.find(catalogName);
    if (found == hosts.end() || !found->second)
        return;
    driver->start(*found->second);
}

void App::cancel()
{
    if (driver)
        driver->cancel();
}

void App::quit()
{
    quitRequested = true;
}

bool App::frame()
{
    window->tick();
    if (driver)
        driver->tick();
    return quitRequested || !appletMainLoop();
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

    driver.reset();
    graph->shutDown();
    nvExit();
    return ran ? EXIT_SUCCESS : EXIT_FAILURE;
}

}
