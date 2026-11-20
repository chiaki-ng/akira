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
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>
#include <arisa/runtime/overlay_handle.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/theme/colour.hpp>

#include <array>
#include <atomic>
#include <functional>
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

#include "accounts/ui/account_model.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/shell_screen.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/theme_packs.hpp"
#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "cloud/cloud_settings.hpp"
#include "network/network_settings.hpp"
#include "network/tunnel.hpp"
#include "network/ui/tunnel_control.hpp"
#include "play/button_capture.hpp"
#include "play/switch_button.hpp"
#include "play/ui/mapping.hpp"
#include "settings/developer_settings.hpp"
#include "settings/maintenance.hpp"
#include "settings/pad_bench.hpp"
#include "settings/ui/settings_model.hpp"
#include "settings/ui/settings_screen.hpp"
#include "consoles/console_list.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "fake_account_desk.hpp"
#include "fake_couch_table.hpp"
#include "fake_ui_dispatcher.hpp"
#include "log/ui/log_shelf.hpp"
#include "play/ui/couch.hpp"
#include "play/ui/couch_prompt.hpp"
#include "play/ui/pad_picker.hpp"
#include "play/ui/seat_passcode.hpp"
#include "network/nat_report.hpp"
#include "network/ui/nat_check.hpp"
#include "play/controller_settings.hpp"
#include "play/remap_target.hpp"
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
    void setUpAccount() override {}
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

class QuietMaintenance final : public akira::settings::Maintenance {
public:
    auto legacyProfile() const -> bool override { return false; }
    auto localSubnet() const -> std::string override { return "192.168.20.0/24"; }
    void restartDiscovery() override {}
    void flushTrophyCache() override {}
    void accountNameDisplayChanged() override {}
    void consolesChanged() override {}
    void probeTls(std::function<void(std::string)> report) override { report("PASS - service verifies CA, hostname and expiry"); }
    void checkForUpdate(std::string, std::function<void(UpdateCheck)> done) override
    {
        done(UpdateCheck{ .outcome = UpdateCheck::Outcome::UpToDate });
    }
    void simulateUpdate(std::string, std::function<void(UpdateCheck)> done) override
    {
        done(UpdateCheck{ .outcome = UpdateCheck::Outcome::Available, .version = "9.9.9" });
    }
    void installUpdate(std::function<void(UpdateProgress)>) override {}
    void cancelUpdate() override {}
    void quit() override {}
    auto discoveryLog() const -> DiscoveryLog override
    {
        return DiscoveryLog{ .version = 3,
            .lines = { "21:40:12 [DISCOVERY] probe 192.168.20.255:9302", "21:40:12 [DISCOVERY] reply 192.168.20.40 PS5 ready",
                "21:40:13 [DISCOVERY] sweep 192.168.30.0/24 (other subnet)", "21:40:14 [DISCOVERY] reply 192.168.30.12 PS4 standby" },
            .enabled = true };
    }
    auto discoveryLogVersion() const -> std::uint64_t override { return 3; }
    void clearDiscoveryLog() override {}
    void runBenchmark(std::function<void(std::string)> line, std::function<void()> done) override
    {
        for (const char* shown : { "========================================", "        GHASH Benchmark Results",
                 "========================================", "", "Iterations per test: 10000", "",
                 "----------------------------------------", "Testing 64 byte blocks...", "",
                 "  [PMULL] Running 10000 iterations...", "  [PMULL] 412305 ops/s | 25.17 MB/s | 24.3 ms total" })
            line(shown);
        done();
    }
    void stopBenchmark() override {}
};

class SamplePads final : public akira::settings::PadBench {
public:
    auto scan() -> std::vector<Pad> override
    {
        return {
            Pad{ .label = "Pro Controller", .key = "switch", .summary = Summary::SwitchProfile, .tunesMotors = true },
            Pad{ .label = "DualSense", .key = "dualsense", .summary = Summary::Native, .direct = true, .tunesMotors = true,
                .drivesLightbar = true },
            Pad{ .label = "Joy-Con (L/R)", .key = "joycon", .summary = Summary::DefaultProfile, .tunesMotors = true,
                .joyCon = true },
        };
    }
    void use(std::size_t, const akira::play::RumbleProfile&) override {}
    void refreshOutputGates(std::size_t) override {}
    void buzz(std::size_t, const akira::play::RumbleProfile&) override {}
};

class ScriptedCapture final : public akira::play::ButtonCapture {
public:
    void begin() override {}
    auto held() -> std::uint64_t override
    {
        return akira::play::bits(akira::play::SwitchButton::ZL) | akira::play::bits(akira::play::SwitchButton::A);
    }
    void end() override {}
};

class QuietTunnel final : public akira::network::Tunnel {
public:
    auto configured() const -> bool override { return true; }
    auto connected() const -> bool override { return true; }
    auto address() const -> std::string override { return "10.8.0.6"; }
    auto lastError() const -> std::string override { return {}; }
    auto connect() -> bool override { return true; }
    void disconnect() override {}
    void restartSearch() override {}
};

class QuietNotifier final : public akira::app::Notifier {
public:
    void notify(std::string) override {}
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
        , network(store, [](const std::vector<akira::network::Ipv4Address>&) {})
        , developer(store)
        , appSettings(store)
        , cloudSettings(store)
        , picture(stream, store, false)
        , controls(controllers, store)
        , help(std::filesystem::path(AKIRA_RESOURCES_DIR) / "help", std::string())
        , packs(AKIRA_RESOURCES_DIR)
        , settings(akira::settings::ui::SettingsModel::Parts{
              .store = store,
              .developer = developer,
              .stream = stream,
              .controllers = controllers,
              .network = network,
              .app = appSettings,
              .cloud = cloudSettings,
              .maintenance = maintenance,
              .pads = pads,
              .capture = capture,
              .notifier = notifier,
              .features = akira::settings::DeviceFeatures{ .selfUpdate = true, .statsExport = true, .cloud = true, .accounts = true,
                  .configFile = std::filesystem::path(AKIRA_RESOURCES_DIR) / ".." / "tests" / "arisa" / "sample_akira.toml",
                  .buildInfoFile = std::filesystem::path(AKIRA_RESOURCES_DIR) / ".." / "tests" / "arisa" / "sample_build_info.txt",
                  .logFolder = root / "logs",
                  .takionPs4 = { 9 }, .takionPs5 = { 20 }, .takionPs5Cloud = { 23 }, .takionPsNow = { 12 } },
          })
        , mapping(controllers, store, controls, capture)
        , tunnelControl(dispatcher, tunnel)
        , logs(root / "logs")
    {
        store.finishLoading();
        seedLogs();
        developer.setPowerUserMenuUnlocked(true);
        loadAssets();
        strings.emplace(providers.assets(), StringCatalog::fallbackLocale);
        nat.emplace(
            dispatcher,
            [] {
                return akira::network::NatReport(libchiaking::play::NatType::type2);
            },
            *strings);
        couch.emplace(couchTable, *strings);
        accounts.emplace(akira::accounts::ui::AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = *strings });
        accounts->reload();
        parts = ShellScreen::Parts{
            .shelf = &shelf,
            .consoles = &consoles,
            .stream = &streamCommands,
            .settings = akira::settings::ui::SettingsScreen::Parts{
                .picture = &picture,
                .model = &settings,
                .mapping = &mapping,
                .nat = &*nat,
                .tunnel = &tunnelControl,
                .help = &help,
                .accounts = &*accounts,
                .logs = &logs,
            },
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

    auto settle(int frames = settleFrames) -> bool
    {
        for (int frame = 0; frame < frames; ++frame)
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
                streamCommands, controls, mapping, nullptr, *strings));
    }

    auto playersDock() -> composition::Element
    {
        using Control = akira::play::StreamControls::Control;
        return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom }, video(),
            akira::play::ui::StreamDock(
                akira::play::ui::StreamDock::Session{
                    .console = "Living room",
                    .controls = { Control::Power, Control::Stats, Control::Pad, Control::Players, Control::Motion },
                    .sleepOffered = true,
                    .statsMode = akira::play::StatsOverlayMode::Off,
                    .players = true,
                    .opening = Control::Players,
                },
                streamCommands, controls, mapping, &*couch, *strings));
    }

    auto overVideo(composition::Element shown) -> composition::Element
    {
        return components::ZStack(video(), std::move(shown));
    }

    void seatCouch()
    {
        couchTable.listed = {
            tests::FakeCouchTable::pad(0, "Pro Controller", akira::play::CouchTable::PadKind::SwitchPro, false, false),
            tests::FakeCouchTable::pad(1, "DualSense", akira::play::CouchTable::PadKind::PlayStation, true, true),
            tests::FakeCouchTable::pad(2, "DualShock 4", akira::play::CouchTable::PadKind::PlayStation, true, true),
        };
        couchTable.listed[2].art = "img/pads/dualshock4.png";
        couchTable.choices = { akira::play::CouchProfileChoice{ .profileId = 7, .label = "Couch guest" },
            akira::play::CouchProfileChoice{ .profileId = 8, .label = "mika" },
            akira::play::CouchProfileChoice{ .profileId = 9, .label = "aoi" } };
        couchTable.seatPad(1, 1, akira::play::CouchSeat{ .phase = akira::play::SeatPhase::Playing, .accountLabel = "mika" });
        couch->refresh();
    }

    void streamWithDock()
    {
        using Control = akira::play::StreamControls::Control;
        exclusive.emplace(application->exclusive_frame(composition::ExclusiveFrame::Options{
            .keep = { composition::OverlayLayer::dialog, composition::OverlayLayer::toast } }));
        dockShown.emplace(application->root().overlays().show(composition::OverlayLayer::dialog,
            akira::play::ui::StreamDock(
                akira::play::ui::StreamDock::Session{
                    .console = "Living room",
                    .controls = { Control::Power, Control::Stats, Control::Pad, Control::Motion },
                    .sleepOffered = true,
                    .statsMode = akira::play::StatsOverlayMode::Off,
                },
                streamCommands, controls, mapping, nullptr, *strings),
            runtime::OverlayBehaviour{ .input = composition::OverlayInput::modal, .alignment = composition::Alignment::bottom }));
    }

    auto pad(provider::Buttons held, provider::Stick left = {}) -> bool
    {
        provider::InputFrame down;
        down.pads[0].connected = true;
        down.pads[0].buttons = held;
        down.pads[0].left = left;
        providers.input().push(down);
        if (!settle(6))
            return false;
        provider::InputFrame up;
        up.pads[0].connected = true;
        providers.input().push(up);
        return settle(12);
    }

    auto focused() -> std::optional<composition::NodeId>
    {
        return application->introspection().focus().focused;
    }

    auto focusedWithin(std::string_view key) -> bool
    {
        std::optional<composition::NodeId> at = focused();
        const runtime::Introspection& look = application->introspection();
        while (at)
        {
            const std::optional<runtime::Introspection::NodeRecord> record = look.node(*at);
            if (!record)
                return false;
            if (record->key == key)
                return true;
            at = record->parent;
        }
        return false;
    }

    auto settingsModel() -> akira::settings::ui::SettingsModel& { return settings; }
    auto board() -> akira::play::ui::Mapping& { return mapping; }
    auto couchModel() -> akira::play::ui::Couch& { return *couch; }
    auto accountModel() -> akira::accounts::ui::AccountModel& { return *accounts; }
    auto accountDesk() -> tests::FakeAccountDesk& { return desk; }
    auto table() -> tests::FakeCouchTable& { return couchTable; }
    auto text() -> const StringCatalog& { return *strings; }

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
        add("img/icon.jpg", resources / "img" / "icon.jpg");
        for (const auto& entry : std::filesystem::directory_iterator(resources / "platform" / "nx" / "img" / "pads"))
            if (entry.path().extension() == ".png")
                add("img/pads/" + entry.path().filename().string(), entry.path());
        for (const char* folder : { "ps5", "buttons" })
            for (const auto& entry : std::filesystem::directory_iterator(resources / "img" / folder))
                add("img/" + std::string(folder) + "/" + entry.path().filename().string(), entry.path());
        add("strings/en.toml", resources / "strings" / "en.toml");
    }

    void seedLogs()
    {
        const std::filesystem::path folder = root / "logs";
        std::filesystem::create_directories(folder / "connections");
        std::ofstream(folder / "akira.log") << "21:40:12 [INFO] akira 2.4 starting on nx\n"
                                               "21:40:12 [INFO] [ARISA] ui thread core 0\n"
                                               "21:40:13 [DEBUG] discovery reply 192.168.20.40 PS5 ready\n"
                                               "21:40:13 [WARN] console clock is 4 s ahead\n"
                                               "21:40:14 [ERROR] takion: retransmit window exhausted\n"
                                               "    at Connection::feed\n"
                                               "21:40:15 [INFO] stream recovered\n";
        std::ofstream(folder / "connections" / "living-room.log") << "21:41:02 [INFO] rtt 12 ms loss 0.0%\n";
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
    akira::network::NetworkSettings network;
    akira::settings::DeveloperSettings developer;
    akira::app::AppSettings appSettings;
    akira::cloud::CloudSettings cloudSettings;
    akira::settings::ui::PictureOptions picture;
    akira::settings::ui::ControlOptions controls;
    akira::settings::HelpCatalog help;
    ThemePacks packs;
    QuietMaintenance maintenance;
    SamplePads pads;
    ScriptedCapture capture;
    QuietNotifier notifier;
    QuietTunnel tunnel;
    tests::FakeUiDispatcher dispatcher;
    akira::settings::ui::SettingsModel settings;
    akira::play::ui::Mapping mapping;
    akira::network::ui::TunnelControl tunnelControl;
    QuietConsoles consoles;
    QuietStream streamCommands;
    std::optional<StringCatalog> strings;
    std::optional<akira::network::ui::NatCheck> nat;
    ShellScreen::Parts parts;
    std::optional<runtime::Application> application;
    std::optional<composition::ExclusiveFrame> exclusive;
    std::optional<runtime::OverlayHandle> dockShown;
    tests::FakeCouchTable couchTable;
    std::optional<akira::play::ui::Couch> couch;
    tests::FakeAccountDesk desk;
    std::optional<akira::accounts::ui::AccountModel> accounts;
    akira::log::ui::LogShelf logs;
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
    constexpr std::array names{ "settings_account", "settings_general", "settings_picture", "settings_controls",
        "settings_updates", "settings_network", "settings_debug", "settings_config", "settings_power_user",
        "settings_developer", "settings_build_info", "settings_about" };
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.showSettings(true);
    REQUIRE(shots.start(shots.shell()));
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, index)));
        CHECK(shots.write(names[index]));
    }
    shots.checkNat();
    REQUIRE(shots.settle());
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 5)));
    CHECK(shots.write("settings_network_checked"));
}

TEST(arisa_shot_settings_details)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.showSettings(true);
    shots.settingsModel().scanPads();
    shots.settingsModel().open(akira::settings::ui::SettingsModel::Detail::Mapping);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("settings_mapping"));
    shots.board().capture(akira::play::remapKey(akira::play::RemapTarget::Cross));
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_mapping_capture"));
    shots.board().cancel();
    shots.settingsModel().open(akira::settings::ui::SettingsModel::Detail::Pad, 1);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_pad_dualsense"));
    shots.settingsModel().open(akira::settings::ui::SettingsModel::Detail::Pad, 0);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_pad_pro"));
    shots.settingsModel().close();
    shots.settingsModel().ask(akira::settings::ui::SettingsModel::Entry::Subnet);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_subnet_entry"));
}

TEST(arisa_settings_capture_assigns_the_combo)
{
    Shots shots;
    akira::play::ui::Mapping& board = shots.board();
    const std::uint32_t cross = akira::play::remapKey(akira::play::RemapTarget::Cross);
    board.capture(cross);
    REQUIRE(board.view().get().capturing.has_value());
    board.cancel();
    CHECK(!board.view().get().capturing.has_value());
    board.reset(cross);
    CHECK(board.board().isDefault(cross));
}

TEST(arisa_shot_stream_dock)
{
    Shots shots;
    REQUIRE(shots.start(shots.dock()));
    CHECK(shots.write("stream_dock_stats"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 1)));
    CHECK(shots.write("stream_dock_controls"));
    REQUIRE(shots.press("stream-dock.mapping-open"));
    CHECK(shots.write("stream_dock_mapping"));
    REQUIRE(shots.press("stream-dock.mapping-back"));
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

TEST(arisa_stream_dock_focus_moves_with_the_dock_buttons)
{
    Shots shots;
    REQUIRE(shots.start(Shots::video()));
    shots.streamWithDock();
    REQUIRE(shots.settle());
    CHECK(shots.focusedWithin("stream-dock.panel"));
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::dpad_down }));
    CHECK(shots.focusedWithin("stream-dock.tabs"));
    const std::optional<composition::NodeId> start = shots.focused();
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::dpad_left }));
    const std::optional<composition::NodeId> left = shots.focused();
    CHECK(left.has_value() && left != start);
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = 1.0F, .y = 0.0F }));
    CHECK(shots.focused() == start);
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = 1.0F, .y = 0.0F }));
    CHECK(shots.focused() != start);
    CHECK(shots.focusedWithin("stream-dock.tabs"));
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::dpad_up }));
    CHECK(shots.focusedWithin("stream-dock.panel"));
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = 0.0F, .y = -1.0F }));
    CHECK(shots.focusedWithin("stream-dock.tabs"));
}


TEST(arisa_shot_couch_players)
{
    Shots shots;
    shots.seatCouch();
    REQUIRE(shots.start(shots.playersDock()));
    CHECK(shots.write("couch_players"));
    shots.table().progress = 0.0F;
    shots.couchModel().invite();
    REQUIRE(shots.settle());
    CHECK(shots.couchModel().view().get().mode == akira::play::ui::Couch::Mode::ChoosingProfile);
    CHECK(shots.write("couch_choose_profile"));
    shots.couchModel().selectProfile(9);
    shots.table().progress = 0.35F;
    shots.couchModel().refresh();
    REQUIRE(shots.settle());
    CHECK(shots.couchModel().view().get().mode == akira::play::ui::Couch::Mode::Claiming);
    CHECK(shots.write("couch_claiming"));
}

TEST(arisa_shot_couch_refused_seat)
{
    Shots shots;
    shots.seatCouch();
    shots.table().seatPad(2, 2, akira::play::CouchSeat{ .phase = akira::play::SeatPhase::Failed,
        .refusal = libchiaking::play::SeatRefusal::account_not_on_console(libchiaking::controllers::SeatNumber::masked(2)),
        .profileId = 7 });
    shots.couchModel().refresh();
    REQUIRE(shots.start(shots.playersDock()));
    CHECK(shots.write("couch_refused"));
}

TEST(arisa_shot_couch_pad_picker)
{
    Shots shots;
    shots.seatCouch();
    const auto now = akira::play::ui::Couch::Clock::now();
    shots.couchModel().askPad(shots.table().pads(), now, [](akira::play::ui::Couch::Pick, std::optional<akira::play::PadId>) {});
    shots.table().picked.clear();
    shots.couchModel().tick(now);
    shots.table().picked = { 1 };
    shots.couchModel().tick(now);
    REQUIRE(shots.start(shots.overVideo(akira::play::ui::PadPicker(shots.couchModel(), shots.text()))));
    CHECK(shots.couchModel().view().get().picker->selected == std::optional<std::size_t>(1));
    CHECK(shots.write("couch_pad_picker"));
}

TEST(arisa_shot_couch_arrival_and_passcode)
{
    Shots shots;
    shots.seatCouch();
    shots.couchModel().arrived([](akira::play::ui::Couch::Arrival) {});
    REQUIRE(shots.start(shots.overVideo(akira::play::ui::CouchPrompt(shots.couchModel(), shots.text()))));
    CHECK(shots.write("couch_arrival"));
    Shots passcode;
    passcode.seatCouch();
    passcode.couchModel().askPasscode(1, true, [] {});
    REQUIRE(passcode.start(passcode.overVideo(akira::play::ui::SeatPasscode(passcode.couchModel(), passcode.text()))));
    CHECK(passcode.write("couch_passcode"));
}

TEST(arisa_shot_onboarding)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.accountModel().welcome(false, true);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("onboarding_welcome"));
    shots.accountModel().startQr();
    shots.accountDesk().qr(akira::accounts::AccountDesk::QrProgress{ .step = akira::accounts::AccountDesk::QrStep::Waiting,
        .url = "https://my.account.sony.com/central/signin/qr?code=482113&client=akira",
        .expires = std::chrono::system_clock::now() + std::chrono::seconds{ 299 } });
    REQUIRE(shots.settle());
    CHECK(shots.write("onboarding_qr"));
    shots.accountDesk().qr(akira::accounts::AccountDesk::QrProgress{ .step = akira::accounts::AccountDesk::QrStep::Failed,
        .failureKey = "akira/qr_sign_in/status_declined" });
    REQUIRE(shots.settle());
    CHECK(shots.write("onboarding_qr_failed"));
    shots.accountModel().startCompanion();
    REQUIRE(shots.settle());
    CHECK(shots.write("onboarding_companion"));
    shots.accountModel().startLegacy();
    shots.accountModel().createLegacy("bad", "");
    REQUIRE(shots.settle());
    CHECK(shots.write("onboarding_legacy"));
}

TEST(arisa_shot_settings_account_dialogs)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.showSettings(true);
    REQUIRE(shots.start(shots.shell()));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 0)));
    shots.accountModel().askRemove(2);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_account_remove"));
    shots.accountModel().dismissRemove();
    shots.accountModel().reveal(true);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_account_revealed"));
}

TEST(arisa_shot_consoles_set_up)
{
    Shots shots;
    shots.model.ready();
    shots.shelf.show(ConsoleList({}, ConsoleList::Profile::None));
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("consoles_set_up"));
}

TEST(arisa_shot_settings_tools)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.showSettings(true);
    REQUIRE(shots.start(shots.shell()));
    shots.settingsModel().open(akira::settings::ui::SettingsModel::Detail::Logs);
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_logs"));
    REQUIRE(shots.press("logs.entry.akira.log"));
    CHECK(shots.write("settings_log_file"));
    shots.settingsModel().openDiscoveryLog();
    REQUIRE(shots.settle());
    CHECK(shots.write("settings_discovery_log"));
    shots.settingsModel().runBenchmark();
    REQUIRE(shots.settle());
    CHECK(!shots.settingsModel().view().get().benchmarking);
    CHECK(shots.settingsModel().view().get().benchmark.size() == 11);
    CHECK(shots.write("settings_benchmark"));
    shots.settingsModel().close();
    REQUIRE(shots.settle());
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 11)));
    REQUIRE(shots.press("about.link.chiaki-ng"));
    CHECK(shots.write("settings_about_qr"));
}

TEST(arisa_settings_discovery_log_follows_the_journal)
{
    Shots shots;
    akira::settings::ui::SettingsModel& settings = shots.settingsModel();
    settings.openDiscoveryLog();
    CHECK(settings.view().get().detail == akira::settings::ui::SettingsModel::Detail::DiscoveryLog);
    CHECK(settings.view().get().discovery.lines.size() == 4);
    CHECK(settings.view().get().discovery.enabled);
    settings.refreshDiscoveryLog();
    CHECK(settings.view().get().discovery.version == 3);
    settings.close();
    CHECK(settings.view().get().detail == akira::settings::ui::SettingsModel::Detail::None);
}
