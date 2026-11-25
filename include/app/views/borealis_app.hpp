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
#include "play/audio_manager.hpp"

namespace akira::app::views {

class BorealisApp {
public:
    BorealisApp(int argc, char* argv[]);

    BorealisApp(const BorealisApp&) = delete;
    BorealisApp& operator=(const BorealisApp&) = delete;

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
    BorealisDispatcher dispatcher;
    BorealisWindow window;
    BorealisStrings strings;
    BorealisGpu gpu;
    BorealisRumble rumble;
    BorealisOverlayPalette overlayPalette;
    play::AudioManager audio;
    ServiceGraph graph;
    std::optional<ViewServices> viewServices;
};

}
