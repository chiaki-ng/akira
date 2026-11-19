#pragma once

#include <memory>
#include <optional>
#include <string>

#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/provider/horizon/horizon_threading.hpp>
#include <arisa/runtime/application.hpp>

#include "app/arisa/dispatcher.hpp"
#include "app/arisa/gpu.hpp"
#include "app/arisa/overlay_palette.hpp"
#include "app/arisa/shell_commands.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/stream_driver.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/window.hpp"
#include "app/service_graph.hpp"
#include "platform/rumble/npad_rumble.hpp"

namespace akira::app::arisa {

class App final : public ShellCommands {
public:
    App();
    ~App() override;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

    void connect(const std::string& catalogName) override;
    void cancel() override;
    void quit() override;

private:
    [[nodiscard]] bool openProviders();
    [[nodiscard]] bool openApplication();
    void startServices();
    void followConsoles();
    bool frame();

    ::arisa::provider::horizon::HorizonThreadingProvider threading;
    std::optional<::arisa::provider::horizon::HorizonProviderFactory> providers;
    std::optional<Window> window;
    std::optional<StringCatalog> strings;
    std::optional<Gpu> gpu;
    platform::NpadRumble rumble;
    OverlayPalette overlayPalette;
    ShellModel model;
    std::optional<::arisa::runtime::Application> application;
    std::optional<Dispatcher> dispatcher;
    std::optional<ServiceGraph> graph;
    std::shared_ptr<StreamDriver> driver;
    bool quitRequested = false;
};

}
