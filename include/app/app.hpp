#pragma once

#include <optional>

#include <switch.h>

#include "app/service_graph.hpp"
#include "app/view_services.hpp"
#include "app/views/borealis_dispatcher.hpp"
#include "app/views/borealis_gpu.hpp"
#include "app/views/borealis_overlay_palette.hpp"
#include "app/views/borealis_rumble.hpp"
#include "app/views/borealis_strings.hpp"
#include "app/views/borealis_window.hpp"

namespace akira::app {

class App {
public:
    App(int argc, char* argv[]);

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    static Result prepareProcess();
    static const char* appletTypeName(AppletType type);

    void readArguments();
    void prepareLocale();
    [[nodiscard]] bool startFrameworks();
    void registerViews();
    void openWindow();
    void runMainLoop();

    int argc;
    char** argv;
    Result mainPlaced;
    views::BorealisDispatcher dispatcher;
    views::BorealisWindow window;
    views::BorealisStrings strings;
    views::BorealisGpu gpu;
    views::BorealisRumble rumble;
    views::BorealisOverlayPalette overlayPalette;
    ServiceGraph graph;
    std::optional<ViewServices> viewServices;
};

}
