#pragma once

#include <memory>
#include <optional>
#include <string>

#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/provider/horizon/horizon_threading.hpp>
#include <arisa/runtime/application.hpp>

#include "app/arisa/dispatcher.hpp"
#include "app/arisa/gpu.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/overlay_palette.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/shell_screen.hpp"
#include "app/arisa/stream_driver.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/window.hpp"
#include "app/service_graph.hpp"
#include "consoles/ui/console_operations.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "network/ui/nat_check.hpp"
#include "platform/rumble/npad_rumble.hpp"
#include "settings/help_catalog.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"

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

    [[nodiscard]] bool openProviders();
    [[nodiscard]] bool openApplication();
    void startServices();
    void buildScreens();
    bool frame();

    ::arisa::provider::horizon::HorizonThreadingProvider threading;
    std::optional<::arisa::provider::horizon::HorizonProviderFactory> providers;
    std::optional<Window> window;
    std::optional<StringCatalog> strings;
    std::optional<Gpu> gpu;
    platform::NpadRumble rumble;
    OverlayPalette overlayPalette;
    ShellModel model;
    Notices notices;
    consoles::ui::ConsoleShelf shelf;
    ShellScreen::Parts parts;
    std::optional<::arisa::runtime::Application> application;
    std::optional<Dispatcher> dispatcher;
    std::optional<ServiceGraph> graph;
    std::optional<settings::HelpCatalog> help;
    std::optional<settings::ui::PictureOptions> picture;
    std::optional<settings::ui::ControlOptions> controls;
    std::optional<network::ui::NatCheck> nat;
    std::shared_ptr<StreamDriver> driver;
    std::optional<consoles::ui::ConsoleOperations> consoleOperations;
};

}
