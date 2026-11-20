#include "test_util.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/theme/colour.hpp>

#include <atomic>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/arisa/notices.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/shell_screen.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/theme_packs.hpp"
#include "consoles/console_list.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "fake_ui_dispatcher.hpp"
#include "network/nat_report.hpp"
#include "network/ui/nat_check.hpp"
#include "play/controller_settings.hpp"
#include "play/stream_settings.hpp"
#include "play/ui/stream_commands.hpp"
#include "play/ui/stream_dock.hpp"
#include "settings/help_catalog.hpp"
#include "settings/settings_store.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"

namespace {

namespace appearance = arisa::appearance;
namespace components = arisa::components;
namespace composition = arisa::composition;
namespace graphics = arisa::graphics;
namespace provider = arisa::provider;
namespace runtime = arisa::runtime;
namespace structure = arisa::structure;
namespace theme = arisa::theme;

using akira::app::arisa::Notices;
using akira::app::arisa::ShellModel;
using akira::app::arisa::ShellScreen;
using akira::app::arisa::StringCatalog;
using akira::app::arisa::ThemePacks;
using akira::consoles::ConsoleEntry;
using akira::consoles::ConsoleFacts;
using akira::consoles::ConsoleList;
using akira::consoles::HostType;
using akira::consoles::ui::ConsoleShelf;

class QuietConsoles final : public akira::consoles::ui::ConsoleCommands {
public:
    void primary(const std::string&) override {}
    void option(const std::string&, ConsoleEntry::Option) override {}
    void wake(const std::string&) override {}
    void registerOverInternet(const std::string&) override {}
    void askPin(const std::string&) override {}
    void registerWithPin(const std::string&, const std::string&) override {}
    void cancelRegistration(const std::string&) override {}
    void link(const std::string&, const std::string&) override {}
    void setConsolePin(const std::string&, const std::string&) override {}
    void setAddress(const std::string&, const std::string&) override {}
    void pinTakion(const std::string&, int) override {}
    void remove(const std::string&) override {}
    void addManual(const std::string&, const std::string&, bool) override {}
    void findRemote() override {}
    auto findsRemote() const -> bool override { return true; }
};

class QuietStream final : public akira::play::ui::StreamCommands {
public:
    void cancel() override {}
    void reconnect() override {}
    void resume() override {}
    void leave(bool) override {}
    void resetMotion() override {}
    void showStats(akira::play::StatsOverlayMode) override {}
    auto stats() -> akira::play::StreamStats override
    {
        akira::play::StreamStats sampled{};
        sampled.fps = 59.9F;
        sampled.measured_bitrate_mbps = 24.6F;
        sampled.rtt_valid = true;
        sampled.rtt_ms = 12.0F;
        sampled.packet_loss_percent = 0.2F;
        sampled.network_frames_lost = 3;
        sampled.requested_fps = 60;
        sampled.latency_valid = true;
        sampled.total_ms = 34.0F;
        sampled.net_ms = 12.0F;
        sampled.visual_ms = 8.4F;
        sampled.jitter_ms = 1.2F;
        sampled.source_fps = 60.0F;
        return sampled;
    }
    auto decoder() -> std::string override { return "NVTEGRA"; }
};

class Shots {
public:
    static constexpr int settleFrames = 90;
    static constexpr std::chrono::milliseconds frameStep{ 16 };
    static constexpr provider::SurfaceSize handheld{ .width = provider::SurfaceWidth{ 1280 },
        .height = provider::SurfaceHeight{ 720 } };

    Shots()
        : root(std::filesystem::temp_directory_path() / ("akira-shots-" + std::to_string(counter++)))
        , store(root)
        , stream(store, false)
        , controllers(store)
        , picture(stream, store, false)
        , controls(controllers, store)
        , help(std::filesystem::path(AKIRA_RESOURCES_DIR) / "help", std::string())
        , packs(AKIRA_RESOURCES_DIR)
    {
        store.finishLoading();
        loadAssets();
        strings.emplace(providers.assets(), StringCatalog::fallbackLocale);
        nat.emplace(
            dispatcher,
            [] {
                return akira::network::NatReport(libchiaking::play::NatType::type2);
            },
            *strings);
        parts = ShellScreen::Parts{
            .shelf = &shelf,
            .consoles = &consoles,
            .stream = &streamCommands,
            .picture = &picture,
            .controls = &controls,
            .nat = &*nat,
            .help = &help,
        };
    }

    ~Shots()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    Shots(const Shots&) = delete;
    Shots& operator=(const Shots&) = delete;

    auto shell() -> composition::Element { return ShellScreen(model, notices, parts, *strings); }

    auto start(composition::Element screen, std::string_view theme = ThemePacks::standard,
        appearance::ColorSchemePreference scheme = appearance::ColorSchemePreference::dark) -> bool
    {
        providers.surface().set_display_mode(provider::DisplayMode::handheld);
        providers.surface().resize(handheld);
        providers.gpu().rasterize(handheld);
        runtime::AppearanceOptions look = packs.options(theme);
        look.policy.defaults.color_scheme = scheme;
        auto created = runtime::Application::create(providers, std::move(screen),
            runtime::ApplicationOptions{ .appearance = std::move(look) });
        if (!created)
            return false;
        application.emplace(std::move(*created));
        return settle();
    }

    auto settle() -> bool
    {
        for (int frame = 0; frame < settleFrames; ++frame)
        {
            if (!providers.clock().advance(frameStep).has_value() || !application->frame().has_value())
                return false;
        }
        return true;
    }

    void checkNat() { nat->start(); }

    auto press(std::string_view key) -> bool
    {
        return application->root().activate(composition::ElementKey{ std::string(key) }) && settle();
    }

    auto themeLoaded() -> bool
    {
        bool clean = true;
        for (const provider::Diagnostic& reported : providers.diagnostics().snapshot())
        {
            if (reported.category != "theme")
                continue;
            std::printf("      theme: %s\n", reported.message.c_str());
            clean = false;
        }
        return clean;
    }

    auto write(std::string_view name) -> bool
    {
        const auto image = providers.gpu().capture();
        if (!image)
            return false;
        std::filesystem::create_directories(AKIRA_SHOTS_DIR);
        const std::string path = (std::filesystem::path(AKIRA_SHOTS_DIR) / (std::string(name) + ".png")).string();
        const int width = static_cast<int>(image->width());
        const int height = static_cast<int>(image->height());
        return stbi_write_png(path.c_str(), width, height, 4, image->pixels().data(), width * 4) != 0;
    }

    static auto facts(std::string name, bool ps5, bool paired, bool standby) -> ConsoleFacts
    {
        ConsoleFacts made;
        made.catalogName = name;
        made.name = std::move(name);
        made.type = HostType::Discovered;
        made.ps5 = ps5;
        made.paired = paired;
        made.discovered = true;
        made.standby = standby;
        made.ready = !standby;
        made.hasAddress = true;
        made.presence = standby ? "Standby" : "Ready";
        return made;
    }

    void showConsoles(std::vector<ConsoleFacts> listed)
    {
        shelf.show(ConsoleList(std::move(listed), ConsoleList::Profile::Signed));
    }

    void showHouse()
    {
        showConsoles({ facts("Living room", true, true, false), facts("Bedroom", false, true, true),
            facts("Study", true, false, false) });
    }

    static auto video() -> composition::Element
    {
        const graphics::Gradient sky(std::vector<theme::ColourValue>{ theme::Colour::hex(0x7FB6D9),
            theme::Colour::hex(0x1C3346), theme::Colour::hex(0x0D1A24) });
        return composition::Element(components::ZStack())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(graphics::ShapeStyle(graphics::LinearGradient{ .gradient = sky }));
    }

    auto dock() -> composition::Element
    {
        using Control = akira::play::StreamControls::Control;
        return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom }, video(),
            akira::play::ui::StreamDock(
                akira::play::ui::StreamDock::Session{
                    .console = "Living room",
                    .controls = { Control::Power, Control::Stats, Control::Pad, Control::Motion },
                    .sleepOffered = true,
                    .statsMode = akira::play::StatsOverlayMode::Compact,
                },
                streamCommands, controls, *strings));
    }

    ShellModel model;
    Notices notices;
    ConsoleShelf shelf;

private:
    void loadAssets()
    {
        const std::filesystem::path fonts = std::filesystem::path(ARISA_ASSETS_DIR) / "arisa" / "fonts";
        const std::filesystem::path resources(AKIRA_RESOURCES_DIR);
        add("arisa/fonts/InterVariable.ttf", fonts / "InterVariable.ttf");
        add("arisa/fonts/MaterialSymbolsRounded.ttf", fonts / "MaterialSymbolsRounded.ttf");
        add("arisa/fonts/MaterialSymbolsRoundedFilled.ttf", fonts / "MaterialSymbolsRoundedFilled.ttf");
        add("img/console/ps5.png", resources / "img" / "console" / "ps5.png");
        add("img/console/ps4.png", resources / "img" / "console" / "ps4.png");
        add("strings/en.toml", resources / "strings" / "en.toml");
    }

    void add(std::string_view at, const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        const std::vector<char> raw{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        std::vector<std::byte> bytes(raw.size());
        std::ranges::transform(raw, bytes.begin(), [](char value) { return std::byte(value); });
        if (auto path = provider::AssetPath::create(at))
            providers.assets().add(std::move(*path), bytes);
    }

    static inline std::atomic<int> counter{ 0 };
    provider::headless::HeadlessProviderFactory providers;
    std::filesystem::path root;
    akira::settings::SettingsStore store;
    akira::play::StreamSettings stream;
    akira::play::ControllerSettings controllers;
    akira::settings::ui::PictureOptions picture;
    akira::settings::ui::ControlOptions controls;
    akira::settings::HelpCatalog help;
    ThemePacks packs;
    tests::FakeUiDispatcher dispatcher;
    QuietConsoles consoles;
    QuietStream streamCommands;
    std::optional<StringCatalog> strings;
    std::optional<akira::network::ui::NatCheck> nat;
    ShellScreen::Parts parts;
    std::optional<runtime::Application> application;
};

}

TEST(arisa_shot_consoles)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("consoles"));
}

TEST(arisa_shot_consoles_single)
{
    Shots shots;
    shots.model.ready();
    shots.showConsoles({ Shots::facts("Living room", true, true, false) });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("consoles_single"));
}

TEST(arisa_shot_consoles_empty)
{
    Shots shots;
    shots.model.ready();
    shots.showConsoles({});
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("consoles_empty"));
}

TEST(arisa_shot_toast)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    REQUIRE(shots.start(shots.shell()));
    shots.notices.notify("Wake-up signal sent to Bedroom");
    REQUIRE(shots.settle());
    CHECK(shots.write("toast"));
}

TEST(arisa_shot_add_console)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.shelf.openAdd(true);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("add_console"));
}

TEST(arisa_shot_pin)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.shelf.ask(ConsoleShelf::Prompt{ .ask = ConsoleShelf::Ask::Pin, .catalogName = "Study" });
    REQUIRE(shots.start(shots.shell()));
    REQUIRE(shots.press("pin.key.4"));
    REQUIRE(shots.press("pin.key.8"));
    REQUIRE(shots.press("pin.key.2"));
    CHECK(shots.write("pin"));
}

TEST(arisa_shot_registration)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.shelf.registering(ConsoleShelf::Registering{ .catalogName = "Study", .stage = "Step 2/4: Finding your console" });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("registration"));
}

TEST(arisa_shot_connecting)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.connecting("Living room");
    shots.model.stage(akira::play::ui::ConnectingScreen::Progress{
        .console = "Living room", .status = "Requesting the session", .step = 3, .steps = 6 });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("connecting"));
}

TEST(arisa_shot_settings_pages)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.showSettings(true);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("settings_picture"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 1)));
    CHECK(shots.write("settings_controls"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 2)));
    CHECK(shots.write("settings_network"));
    shots.checkNat();
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_network_checked"));
}

TEST(arisa_shot_stream_dock)
{
    Shots shots;
    REQUIRE(shots.start(shots.dock()));
    CHECK(shots.write("stream_dock_stats"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 1)));
    CHECK(shots.write("stream_dock_controls"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 2)));
    CHECK(shots.write("stream_dock_session"));
}

TEST(arisa_shot_theme_packs)
{
    for (const std::string_view pack : ThemePacks::names)
    {
        Shots shots;
        shots.model.ready();
        shots.showHouse();
        REQUIRE(shots.start(shots.shell(), pack));
        CHECK(shots.themeLoaded());
        CHECK(shots.write(std::string("theme_") + std::string(pack)));
    }
    Shots light;
    light.model.ready();
    light.showHouse();
    REQUIRE(light.start(light.shell(), "akira-anniversary", appearance::ColorSchemePreference::light));
    CHECK(light.write("theme_akira-anniversary_light"));
}
