#pragma once

#include <cstdarg>
#include <functional>
#include <optional>

#include <switch.h>

#include "app/services.hpp"
#include "app/view_services.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "play/chiaki_core.hpp"
#include "trophies/trophy_manager.hpp"

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
    static void forwardFfmpegLog(void* context, int level, const char* format, va_list arguments);

    void openLog();
    void readArguments();
    void prepareLocale();
    void startServices();
    [[nodiscard]] bool startFrameworks();
    std::function<void()> signInRenewed();
    void startCore();
    void startAsyncLogging();
    void registerViews();
    void openWindow();
    void runMainLoop();
    void shutDown();

    int argc;
    char** argv;
    Result mainPlaced;
    std::optional<trophies::TrophyManager> trophies;
#if AKIRA_CLOUD
    std::optional<cloud::Service> cloud;
#endif
    std::optional<Services> services;
    std::optional<ViewServices> viewServices;
    std::optional<play::ChiakiCore> core;
};

}
