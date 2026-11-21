#include "app/views/borealis_app.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include <cstdlib>
#include <cstring>

#include "accounts/views/psn_gated_box.hpp"
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
#include "log/log.hpp"
#include "network/views/dns_warning_dialog.hpp"
#include "network/views/network_utilities_tab.hpp"
#include "network/wireguard_manager.hpp"
#include "platform/factory.hpp"
#include "play/views/stream_view.hpp"
#include "settings/views/config_view_tab.hpp"
#include "trophies/views/trophy_list_tab.hpp"

namespace akira::app::views {

BorealisApp::BorealisApp(int argc, char* argv[])
    : argc(argc),
      argv(argv),
      mainPlaced(prepareProcess()),
      graph(UiSeams{
          .dispatcher = dispatcher,
          .window = window,
          .strings = strings,
          .gpu = gpu,
          .rumble = rumble,
          .audio = audio,
          .overlayPalette = overlayPalette,
      })
{
}

Result BorealisApp::prepareProcess()
{
    ServiceGraph::prepareProcess();
    return WorkerCores::placeMainThread();
}

const char* BorealisApp::appletTypeName(AppletType type)
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

void BorealisApp::readArguments()
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

void BorealisApp::prepareLocale()
{
    std::string overrideLocale = graph.appSettings().debugLocale();
    if (!overrideLocale.empty()) {
        brls::Platform::APP_LOCALE_DEFAULT = overrideLocale;
    } else {
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    }
}

bool BorealisApp::startFrameworks()
{
    CrashReport::stage("borealis init");
    if (!brls::Application::init())
    {
        log::error("Unable to init Borealis application");
        return false;
    }

    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 369.0f);
    brls::getStyle().addMetric("brls/sidebar/item_font_size", 24.0f);

    if (!graph.startSdl(true))
        return false;

    graph.followFocus();
    return true;
}

void BorealisApp::registerViews()
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

void BorealisApp::openWindow()
{
    CrashReport::stage("create window");
    brls::Application::createWindow("Akira");

    if (!brls::Application::loadFontFromFile("mono", BRLS_ASSET("font/Cousine-Regular.ttf")))
        log::warning("Could not load mono font, stats overlay will fall back to the regular font");

    brls::Application::getPlatform()->exitToHomeMode(false);
}

void BorealisApp::runMainLoop()
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

int BorealisApp::run()
{
    graph.openLog();
    log::info("main thread core {} placement {:#x}", svcGetCurrentProcessorNumber(), mainPlaced);
    readArguments();

    graph.startFfmpegLog();

    prepareLocale();

    CrashReport::stage("theme");
    views::setActiveTheme(graph.appSettings().uiTheme());
    views::applyToBorealis();

    if (!graph.startCurl())
        return EXIT_FAILURE;

    graph.startCore();
    graph.startServices();
    viewServices.emplace(graph.services());

    graph.platform().updates().prepare(argc, argv);
    graph.platform().display().prepare();

    if (!startFrameworks())
        return EXIT_FAILURE;

    graph.startAsyncLogging();

    AppletType appletType = appletGetAppletType();
    log::info("Applet type: {} ({})", appletTypeName(appletType), static_cast<int>(appletType));

    registerViews();

    graph.platform().prepareVideo();

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
    graph.shutDown();
    viewServices.reset();
    nvExit();
    return EXIT_SUCCESS;
}

}
