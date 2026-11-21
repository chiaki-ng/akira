#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <arisa/provider/diagnostics/crash_reporter.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/provider/horizon/horizon_threading.hpp>
#include <arisa/provider/standard/directory_storage.hpp>
#include <arisa/runtime/application.hpp>

#include "app/arisa/art_session.hpp"
#include "app/arisa/couch_bridge.hpp"
#include "app/arisa/dispatcher.hpp"
#include "app/arisa/gpu.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/pad_capture.hpp"
#include "app/arisa/screenshots.hpp"
#include "app/arisa/pad_workbench.hpp"
#include "app/arisa/profile_desk.hpp"
#include "app/arisa/psn_trophy_desk.hpp"
#if AKIRA_CLOUD
#include "app/arisa/psn_cloud_desk.hpp"
#include "cloud/ui/cloud_model.hpp"
#endif
#include "app/arisa/upkeep.hpp"
#include "app/arisa/wireguard_tunnel.hpp"
#include "app/arisa/overlay_palette.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/rumble.hpp"
#include "app/arisa/shell_screen.hpp"
#include "app/arisa/stream_driver.hpp"
#include "app/arisa/stream_stage.hpp"
#include "app/arisa/stream_voice.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/theme_packs.hpp"
#include "app/arisa/window.hpp"
#include "accounts/ui/account_model.hpp"
#include "app/game_art.hpp"
#include "app/service_graph.hpp"
#include "consoles/ui/console_operations.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "consoles/ui/recent_shelf.hpp"
#include "log/ui/log_shelf.hpp"
#include "network/ui/nat_check.hpp"
#include "network/ui/tunnel_control.hpp"
#include "play/ui/couch.hpp"
#include "play/ui/mapping.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"
#include "settings/ui/sound_options.hpp"
#include "settings/ui/settings_model.hpp"
#include "trophies/ui/trophy_model.hpp"

namespace akira::app::arisa {

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    static constexpr const char* helpDirectory = "romfs:/help";
    static constexpr const char* themeFolder = "romfs:/";
    static constexpr const char* appearanceFolder = "sdmc:/switch/akira";
    static constexpr std::string_view crashDirectory = "sdmc:/switch/akira/crashes";
    static constexpr std::string_view crashApp = "akira-arisa";

    void installCrashReporter();
    [[nodiscard]] bool openProviders();
    [[nodiscard]] bool openApplication();
    void startServices();
    void buildScreens();
    void buildCouch();
    void buildPages();
    void profileChanged();
    void offerPages();
    auto features() -> settings::DeviceFeatures;
    bool frame();
    bool firstFrameShown = false;
    bool quitting = false;

    std::optional<::arisa::provider::CrashReporter> crashes;
    ::arisa::provider::horizon::HorizonThreadingProvider threading;
    std::optional<::arisa::provider::horizon::HorizonProviderFactory> providers;
    std::optional<Window> window;
    std::optional<StringCatalog> strings;
    std::optional<Gpu> gpu;
    std::optional<Rumble> rumble;
    std::optional<StreamVoice> voice;
    OverlayPalette overlayPalette;
    ThemePacks themes{ themeFolder };
    ::arisa::provider::standard::DirectoryStorage appearanceStore{ appearanceFolder };
    ShellModel model;
    Notices notices;
    consoles::ui::ConsoleShelf shelf;
    consoles::ui::RecentShelf recent;
    ShellScreen::Parts parts;
    std::optional<GameArt> art;
    std::optional<ArtSession> artSession;
    std::optional<::arisa::runtime::Application> application;
    std::optional<StreamStage> stage;
    std::optional<Dispatcher> dispatcher;
    std::optional<ServiceGraph> graph;
    std::optional<settings::HelpCatalog> help;
    std::optional<settings::ui::PictureOptions> picture;
    std::optional<settings::ui::SoundOptions> sound;
    std::optional<settings::ui::ControlOptions> controls;
    std::optional<network::ui::NatCheck> nat;
    std::optional<Upkeep> upkeep;
    std::optional<PadWorkbench> padBench;
    std::optional<WireGuardTunnel> tunnel;
    std::optional<network::ui::TunnelControl> tunnelControl;
    std::optional<PadCapture> capture;
    std::optional<settings::ui::SettingsModel> settingsModel;
    std::optional<ProfileDesk> desk;
    std::optional<accounts::ui::AccountModel> accountModel;
    std::optional<log::ui::LogShelf> logs;
    std::optional<play::ui::Mapping> mapping;
    std::optional<CouchBridge> couchBridge;
    std::optional<play::ui::Couch> couch;
    std::shared_ptr<StreamDriver> driver;
    std::optional<Screenshots> screenshots;
    std::optional<::arisa::support::ScopedCallback> screenshotWatch;
    std::optional<consoles::ui::ConsoleOperations> consoleOperations;
    std::optional<PsnTrophyDesk> trophyDesk;
    std::optional<trophies::ui::TrophyModel> trophyModel;
#if AKIRA_CLOUD
    std::optional<PsnCloudDesk> cloudDesk;
    std::optional<cloud::ui::CloudModel> cloudModel;
#endif
};

}
