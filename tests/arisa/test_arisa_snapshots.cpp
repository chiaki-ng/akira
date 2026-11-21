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
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>
#include <arisa/runtime/overlay_handle.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/digit_keypad.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/theme/colour.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "accounts/ui/account_model.hpp"
#include "app/arisa/confirmation.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/overlay_palette.hpp"
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
#include "play/ui/login_pin_card.hpp"
#include "consoles/debug_consoles.hpp"
#include "network/nat_report.hpp"
#include "network/ui/nat_check.hpp"
#include "play/controller_settings.hpp"
#include "play/remap_target.hpp"
#include "play/stream_settings.hpp"
#include "play/ui/stream_commands.hpp"
#include "play/ui/stats_board.hpp"
#include "play/ui/diagnostics_overlay.hpp"
#include "play/ui/stream_dock.hpp"
#include "play/stats_hud_placement.hpp"
#include "play/stats_readout.hpp"
#include "settings/help_catalog.hpp"
#include "settings/settings_store.hpp"
#include "settings/ui/control_options.hpp"
#include "settings/ui/picture_options.hpp"
#include "settings/ui/sound_options.hpp"

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
    void primary(const std::string& name) override { primaries.push_back(name); }
    void registerOverInternet(const std::string&) override {}
    void askPin(const std::string&) override {}
    void registerWithPin(const std::string&, const std::string&) override {}
    void cancelRegistration(const std::string&) override {}
    void link(const std::string&, const std::string&) override {}
    void remove(const std::string&) override {}
    void edit(const std::string&) override {}
    void saveEdit(const std::string&, const akira::consoles::ui::ConsoleShelf::Edit&) override {}
    void askDelete(const std::string&) override {}
    void openAdd() override {}
    void chooseAddMode(akira::consoles::ui::ConsoleShelf::AddMode) override {}
    void addByAddress(const std::string&, bool) override {}
    void findRemote() override {}
    void setUpAccount() override {}
    void launchTitle(const std::string&, const std::string&) override {}

    std::vector<std::string> primaries;
};

class QuietStream final : public akira::play::ui::StreamCommands {
public:
    void cancel() override {}
    void reconnect() override {}
    void restart() override {}
    void resume() override {}
    void leave(bool) override {}
    void goHome() override { ++homes; }
    void resetMotion() override {}
    void tuneColour(akira::play::ColourAdjustments chosen) override { colour = chosen; }
    void tuneVolume(akira::play::Volume chosen) override { volume = chosen; }
    void showDiagnostics(akira::play::DiagnosticsAppearance chosen) override { appearance = chosen; }
    auto diagnostics() -> akira::play::DiagnosticsReport override { return report(false); }
    void submitLoginPin(akira::consoles::ConsolePin pin) override { pins.push_back(pin.text()); }
    void cancelLoginPin() override { ++pinCancels; }

    static auto sample() -> akira::play::StreamStats
    {
        akira::play::StreamStats sampled{};
        sampled.fps = 59.9F;
        sampled.measured_bitrate_mbps = 24.6F;
        sampled.rtt_valid = true;
        sampled.rtt_ms = 12.0F;
        sampled.rtt_variation_ms = 1.8F;
        sampled.rtt_samples = 420;
        sampled.rtt_p99_ms = 19.0F;
        sampled.rtt_recent_max_ms = 22.0F;
        sampled.packet_loss_percent = 0.2F;
        sampled.packets_received = 182340;
        sampled.packets_lost = 41;
        sampled.network_frames_lost = 3;
        sampled.frames_recovered = 4;
        sampled.frames_received = 45210;
        sampled.keyframe_requests = 2;
        sampled.fec_repaired = 17;
        sampled.requested_width = 1920;
        sampled.requested_height = 1080;
        sampled.requested_fps = 60;
        sampled.requested_bitrate = 30000;
        sampled.requested_hevc = true;
        sampled.is_hevc = true;
        sampled.is_hardware_decoder = true;
        sampled.video_width = 1920;
        sampled.video_height = 1080;
        sampled.stream_duration_seconds = 754;
        sampled.latency_valid = true;
        sampled.total_ms = 34.0F;
        sampled.net_ms = 12.0F;
        sampled.visual_ms = 8.4F;
        sampled.decode_ms = 4.2F;
        sampled.present_ms = 4.2F;
        sampled.jitter_ms = 1.2F;
        sampled.source_fps = 60.0F;
        return sampled;
    }

    static auto report(bool upscaled) -> akira::play::DiagnosticsReport
    {
        akira::play::DiagnosticsHistory history;
        akira::play::StreamStats stats = sample();
        for (int step = 0; step < 90; ++step)
        {
            stats.measured_bitrate_mbps = 22.0F + static_cast<float>((step * 7) % 9) * 0.6F;
            stats.source_fps = step % 23 == 0 ? 52.0F : 60.0F;
            stats.visual_ms = 7.0F + static_cast<float>((step * 5) % 7) * 0.5F;
            if (step == 40 || step == 71)
                stats.network_frames_lost += 1;
            history.record(stats, 0.5);
        }
        akira::play::DiagnosticsReport::Sample shown{ .stats = sample(), .decoder = "NVTEGRA",
            .context = "PS5 \xc2\xb7 Home network" };
        if (upscaled)
            shown.upscaling = akira::play::UpscalingSummary{ .active = true, .easu = true, .rcas = true, .sharpness = 0.2F,
                .inputHeight = 720, .targetHeight = 1080, .displayHeight = 1080 };
        return akira::play::DiagnosticsReport(std::move(shown), std::move(history));
    }

    akira::play::DiagnosticsAppearance appearance;
    akira::play::ColourAdjustments colour;
    akira::play::Volume volume;
    int homes = 0;
    std::vector<std::string> pins;
    int pinCancels = 0;
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
    static constexpr provider::SurfaceSize switch2Handheld{ .width = provider::SurfaceWidth{ 1920 },
        .height = provider::SurfaceHeight{ 1080 } };

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
        , sound(stream, store)
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
        appearance::ColorSchemePreference scheme = appearance::ColorSchemePreference::dark,
        provider::SurfaceSize size = handheld, provider::DisplayMode mode = provider::DisplayMode::handheld) -> bool
    {
        surfaceSize = size;
        providers.surface().set_display_mode(mode);
        providers.surface().resize(size);
        providers.surface().set_scale(provider::SurfaceScale::for_pixels(size));
        providers.gpu().rasterize(size);
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

    auto pressWithin(std::string_view scope, std::string_view key) -> bool
    {
        const runtime::Introspection& look = application->introspection();
        for (const runtime::Introspection::NodeRecord& record : look.nodes())
            if (record.key == key && within(record.id, scope))
                return application->root().activate(record.id) && settle();
        return false;
    }

    auto settingsTab(std::size_t index) -> bool
    {
        return pressWithin("settings.pages", structure::DockBar::key_of(structure::DockBar::tab_key, index));
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
                    .diagnostics = { .level = akira::play::DiagnosticsAppearance::Level::Compact },
                },
                streamCommands, controls, mapping, nullptr, *strings));
    }

    auto streamDock() -> composition::Element
    {
        using Control = akira::play::StreamControls::Control;
        return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom }, video(),
            akira::play::ui::StreamDock(
                akira::play::ui::StreamDock::Session{
                    .console = "Living room",
                    .controls = { Control::Power, Control::Stats, Control::Pad, Control::Motion },
                    .sleepOffered = true,
                    .homeOffered = true,
                    .diagnostics = {},
                    .profile = akira::play::StreamProfile::Local,
                },
                streamCommands, controls, &picture, &sound, mapping, nullptr, *strings));
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
                    .diagnostics = {},
                    .players = true,
                    .opening = Control::Players,
                },
                streamCommands, controls, mapping, &*couch, *strings));
    }

    auto overVideo(composition::Element shown) -> composition::Element
    {
        return components::ZStack(video(), std::move(shown));
    }

    auto hud(akira::play::DiagnosticsAppearance chosen, bool upscaled) -> composition::Element
    {
        statsBoard.show(chosen);
        statsBoard.refresh(QuietStream::report(upscaled));
        statsBoard.place(hudPlacement.origin());
        return overVideo(akira::play::ui::DiagnosticsOverlay(statsBoard, hudPlacement, *strings));
    }

    void dragHud(akira::play::TouchClaim::Fraction from, akira::play::TouchClaim::Fraction to)
    {
        hudPlacement.show(true);
        if (hudPlacement.grab(from))
        {
            hudPlacement.drag(to);
            hudPlacement.release();
        }
        statsBoard.place(hudPlacement.origin());
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

    auto modal(composition::Element content) -> bool
    {
        modalShown.emplace(application->root().overlays().show(composition::OverlayLayer::dialog, std::move(content),
            runtime::OverlayBehaviour{ .input = composition::OverlayInput::modal, .alignment = composition::Alignment::center }));
        return settle();
    }

    void streamWithDock()
    {
        using Control = akira::play::StreamControls::Control;
        exclusive.emplace(application->exclusive_frame(composition::ExclusiveFrame::Options{
            .keep = { composition::OverlayLayer::dialog, composition::OverlayLayer::toast } }));
        streamBlocked.emplace(application->block_input());
        dockPassed.emplace(application->pass_input(akira::play::ui::StreamDock::passedButtons,
            akira::play::ui::StreamDock::passedInput));
        dockShown.emplace(application->root().overlays().show(composition::OverlayLayer::dialog,
            akira::play::ui::StreamDock(
                akira::play::ui::StreamDock::Session{
                    .console = "Living room",
                    .controls = { Control::Power, Control::Stats, Control::Pad, Control::Motion },
                    .sleepOffered = true,
                    .diagnostics = {},
                    .opening = Control::Power,
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

    auto tap(std::string_view key) -> bool
    {
        const std::optional<composition::Rect> area = areaOf(key);
        if (!area)
            return false;
        const provider::PixelPoint point = pixel(centre(*area));
        return touch(point, provider::TouchPhase::began, 4) && touch(point, provider::TouchPhase::ended, 12);
    }

    auto drag(std::string_view key, float rise) -> bool
    {
        constexpr int steps = 8;
        const std::optional<composition::Rect> area = areaOf(key);
        if (!area)
            return false;
        const composition::Point from = centre(*area);
        if (!touch(pixel(from), provider::TouchPhase::began, 1))
            return false;
        for (int step = 1; step <= steps; ++step)
        {
            const float travelled = rise * static_cast<float>(step) / static_cast<float>(steps);
            if (!touch(pixel(composition::Point{ .x = from.x, .y = from.y - travelled }), provider::TouchPhase::moved, 1))
                return false;
        }
        return touch(pixel(composition::Point{ .x = from.x, .y = from.y - rise }), provider::TouchPhase::ended, 30);
    }

    auto areaOf(std::string_view key) -> std::optional<composition::Rect>
    {
        for (const runtime::Introspection::NodeRecord& record : application->introspection().nodes())
            if (record.key == key)
                return record.bounds.width > 0.0F ? record.bounds : record.frame;
        return std::nullopt;
    }

    auto viewportWidth() -> float { return application->introspection().viewport().width; }

    auto shows(std::string_view key) -> bool
    {
        const std::vector<runtime::Introspection::NodeRecord> all = application->introspection().nodes();
        return std::ranges::any_of(all, [key](const runtime::Introspection::NodeRecord& record) { return record.key == key; });
    }

    auto focused() -> std::optional<composition::NodeId>
    {
        return application->introspection().focus().focused;
    }

    auto focusedWithin(std::string_view key) -> bool
    {
        const std::optional<composition::NodeId> at = focused();
        return at && within(*at, key);
    }

    auto within(composition::NodeId node, std::string_view key) -> bool
    {
        std::optional<composition::NodeId> at = node;
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
    auto consoleCommands() -> QuietConsoles& { return consoles; }
    auto streamControls() -> QuietStream& { return streamCommands; }
    auto pictureOptions() -> akira::settings::ui::PictureOptions& { return picture; }
    auto soundOptions() -> akira::settings::ui::SoundOptions& { return sound; }
    auto streamSettings() -> akira::play::StreamSettings& { return stream; }
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
        add("arisa/fonts/JetBrainsMono-Regular.ttf", fonts / "JetBrainsMono-Regular.ttf");
        add("img/chiaking.png", resources / "img" / "chiaking.png");
        add("img/console/ps5.png", resources / "img" / "console" / "ps5.png");
        add("img/console/ps4.png", resources / "img" / "console" / "ps4.png");
        add("img/icon.jpg", resources / "img" / "icon.jpg");
        for (const auto& entry : std::filesystem::directory_iterator(resources / "img" / "brand"))
            add("img/brand/" + entry.path().filename().string(), entry.path());
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

    static auto centre(composition::Rect area) -> composition::Point
    {
        return composition::Point{ .x = area.x + (area.width * 0.5F), .y = area.y + (area.height * 0.5F) };
    }

    auto pixel(composition::Point point) -> provider::PixelPoint
    {
        const float factor = static_cast<float>(surfaceSize.width.value()) / viewportWidth();
        return provider::PixelPoint{ .x = point.x * factor, .y = point.y * factor };
    }

    auto touch(provider::PixelPoint point, provider::TouchPhase phase, int frames) -> bool
    {
        provider::InputFrame frame;
        frame.touches.push_back(provider::Touch{ .id = provider::TouchId{ 1 }, .position = point, .phase = phase });
        providers.input().push(frame);
        return settle(frames);
    }

    static inline std::atomic<int> counter{ 0 };
    provider::SurfaceSize surfaceSize = handheld;
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
    akira::settings::ui::SoundOptions sound;
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
    akira::play::ui::StatsBoard statsBoard;
    akira::play::StatsHudPlacement hudPlacement;
    std::optional<runtime::Application> application;
    std::optional<composition::ExclusiveFrame> exclusive;
    std::optional<runtime::OverlayHandle> dockShown;
    std::optional<runtime::OverlayHandle> modalShown;
    std::optional<runtime::InputBlock> streamBlocked;
    std::optional<runtime::InputPassthrough> dockPassed;
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
    ConsoleFacts attic = Shots::facts("Attic", true, false, true);
    attic.address = "192.168.20.52";
    ConsoleFacts cabin = Shots::facts("Cabin PS5", true, false, false);
    cabin.type = HostType::Remote;
    cabin.discovered = true;
    cabin.canPairOverInternet = true;
    ConsoleFacts office = Shots::facts("Office PS5", true, false, false);
    office.type = HostType::Remote;
    office.discovered = true;
    office.canPairOverInternet = true;
    office.remotePlayDisabled = true;
    ConsoleFacts study = Shots::facts("Study", true, false, false);
    study.address = "192.168.20.44";
    shots.showConsoles({ Shots::facts("Living room", true, true, false), Shots::facts("Bedroom", false, true, true), study,
        attic, cabin, office });
    shots.shelf.offerAccount(true);
    shots.shelf.openAdd(true);
    shots.shelf.scanned(ConsoleShelf::Scan{ .running = true, .searched = true, .broadcast = "192.168.20.0/24",
        .unicast = { "192.168.30.0/24" }, .address = "192.168.20.31" });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("add-console.nearby.Study"));
    CHECK(shots.shows("add-console.nearby.Attic"));
    CHECK(!shots.shows("Study.primary"));
    CHECK(shots.write("add_console"));
    shots.shelf.chooseMode(ConsoleShelf::AddMode::Manual);
    REQUIRE(shots.settle());
    CHECK(shots.shows("add-console.address"));
    CHECK(shots.write("add_console_manual"));
    shots.shelf.chooseMode(ConsoleShelf::AddMode::PlayStation);
    shots.shelf.lookup(ConsoleShelf::Lookup::Ready);
    REQUIRE(shots.settle());
    CHECK(shots.shows("add-console.account.Cabin PS5"));
    CHECK(shots.write("add_console_playstation"));
    shots.shelf.lookup(ConsoleShelf::Lookup::Failed);
    REQUIRE(shots.settle());
    CHECK(shots.shows("add-console.account-retry"));
    CHECK(shots.write("add_console_playstation_failed"));
}

TEST(arisa_shot_console_editor)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.shelf.edit(ConsoleShelf::Editing{ .catalogName = "Living room",
        .name = "Living room",
        .ps5 = true,
        .values = ConsoleShelf::Edit{ .address = "192.168.20.40", .pin = "1234", .takion = 0 },
        .address = true,
        .pin = true,
        .takion = { "Follow settings (v20)", "v12", "v20" },
        .deletable = true });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("console-editor.save"));
    CHECK(shots.write("console_editor"));
}

TEST(arisa_shot_exit_confirm)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    REQUIRE(shots.start(shots.shell()));
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::b }));
    CHECK(shots.model.view().get().exiting);
    CHECK(shots.shows("exit.confirm"));
    CHECK(shots.write("exit_confirm"));
    REQUIRE(shots.press("exit.cancel"));
    CHECK(!shots.model.view().get().exiting);
    shots.model.askExit();
    REQUIRE(shots.settle());
    REQUIRE(shots.press("exit.confirm"));
    CHECK(shots.model.view().get().exitConfirmed);
}

TEST(arisa_shot_pin)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.shelf.ask(ConsoleShelf::Prompt{ .ask = ConsoleShelf::Ask::Pin, .catalogName = "Study" });
    REQUIRE(shots.start(shots.shell()));
    REQUIRE(shots.press(structure::DigitKeypad::key_of('4')));
    REQUIRE(shots.press(structure::DigitKeypad::key_of('8')));
    REQUIRE(shots.press(structure::DigitKeypad::key_of('2')));
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

TEST(arisa_shot_session_ended_alerts)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.ended(ShellModel::Ending{ .title = "Couldn't connect",
        .message = "The console didn't answer. Check that it's on and on the same network, then try again." });
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.write("alert_connect_failed"));
    Shots reconnect;
    reconnect.model.ready();
    reconnect.showHouse();
    reconnect.model.ended(ShellModel::Ending{ .title = "The stream was lost while akira was in the background. Reconnect?",
        .reconnect = true });
    REQUIRE(reconnect.start(reconnect.shell()));
    CHECK(reconnect.focusedWithin("ending.yes"));
    CHECK(reconnect.write("alert_reconnect"));
    REQUIRE(reconnect.press("ending.no"));
    CHECK(!reconnect.model.view().get().ending.has_value());
    Shots rest;
    bool turnedOff = false;
    std::vector<akira::app::arisa::Confirmation::Choice> choices;
    choices.push_back(akira::app::arisa::Confirmation::Choice{ .label = rest.text().text("akira/stream/keep_on"),
        .chosen = [] {},
        .key = "leave-confirm.keep" });
    choices.push_back(akira::app::arisa::Confirmation::Choice{ .label = rest.text().text("akira/stream/turn_off"),
        .chosen = [&turnedOff] { turnedOff = true; },
        .emphasis = akira::app::arisa::Confirmation::Emphasis::Prominent,
        .key = "leave-confirm.turn-off" });
    REQUIRE(rest.start(rest.overVideo(akira::app::arisa::Confirmation(rest.text().format("akira/stream/turn_off_title",
                                                                           std::string("Living room")),
        rest.text().text("akira/stream/turn_off_message"), std::move(choices), [] {})
                                          .dimmed("leave-confirm"))));
    CHECK(rest.write("alert_rest_on_exit"));
    REQUIRE(rest.press("leave-confirm.turn-off"));
    CHECK(turnedOff);
}

TEST(arisa_shot_stats_hud)
{
    using Appearance = akira::play::DiagnosticsAppearance;
    struct Case {
        const char* name;
        Appearance appearance;
        bool upscaled;
    };
    const std::array cases{
        Case{ "stats_hud_compact", { .level = Appearance::Level::Compact }, false },
        Case{ "stats_hud_compact_clear", { .level = Appearance::Level::Compact, .glass = Appearance::Glass::Clear }, true },
        Case{ "stats_hud_standard_sheet", { .level = Appearance::Level::Standard }, false },
        Case{ "stats_hud_standard_floating",
            { .level = Appearance::Level::Standard, .layout = Appearance::Layout::Floating }, true },
        Case{ "stats_hud_standard_dimmed",
            { .level = Appearance::Level::Standard, .glass = Appearance::Glass::ClearDimmed }, false },
        Case{ "stats_hud_standard_health",
            { .level = Appearance::Level::Standard, .glass = Appearance::Glass::HealthTint, .layout = Appearance::Layout::Floating },
            false },
        Case{ "stats_hud_full_sheet", { .level = Appearance::Level::Full, .glass = Appearance::Glass::Solid }, true },
        Case{ "stats_hud_full_floating",
            { .level = Appearance::Level::Full, .glass = Appearance::Glass::Clear, .layout = Appearance::Layout::Floating }, true },
    };
    for (const Case& shown : cases)
    {
        Shots shots;
        REQUIRE(shots.start(shots.hud(shown.appearance, shown.upscaled)));
        CHECK(shots.write(shown.name));
    }
    Shots dragged;
    REQUIRE(dragged.start(dragged.hud({ .level = Appearance::Level::Standard }, true)));
    dragged.dragHud({ .x = 60.0F / 1280.0F, .y = 60.0F / 720.0F }, { .x = 400.0F / 1280.0F, .y = 300.0F / 720.0F });
    REQUIRE(dragged.settle());
    CHECK(dragged.write("stats_hud_standard_dragged"));
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
        REQUIRE(shots.settingsTab(index));
        CHECK(shots.write(names[index]));
    }
    shots.checkNat();
    REQUIRE(shots.settle());
    REQUIRE(shots.settingsTab(5));
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

TEST(arisa_shot_stream_dock_stream_panel)
{
    Shots shots;
    REQUIRE(shots.start(shots.streamDock()));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 0)));
    CHECK(shots.shows("stream-dock.restart"));
    CHECK(shots.write("stream_dock_stream"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 1)));
    CHECK(shots.shows("stream-dock.akira/stream_menu/colour_gamma"));
    CHECK(shots.write("stream_dock_colour"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 2)));
    CHECK(shots.shows("stream-dock.volume"));
    CHECK(shots.write("stream_dock_sound"));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 5)));
    CHECK(shots.shows("stream-dock.disconnect"));
    CHECK(shots.shows("stream-dock.home"));
    CHECK(shots.write("stream_dock_session_home"));
    REQUIRE(shots.press("stream-dock.home"));
    CHECK_EQ(shots.streamControls().homes, 1);
}

TEST(arisa_stream_dock_colour_and_volume_apply_live_and_persist)
{
    Shots shots;
    REQUIRE(shots.start(shots.streamDock()));
    REQUIRE(shots.press(structure::DockBar::key_of(structure::DockBar::tab_key, 1)));
    CHECK(shots.shows("stream-dock.colour-reset"));
    shots.pictureOptions().setColour(akira::play::ColourAdjustments(0.1, 1.2, 0.8, 1.4));
    REQUIRE(shots.settle());
    CHECK(shots.write("stream_dock_colour_adjusted"));
    REQUIRE(shots.press("stream-dock.colour-reset"));
    CHECK(shots.streamControls().colour.isNeutral());
    CHECK(shots.pictureOptions().view().get().colour.isNeutral());
    CHECK(shots.streamSettings().colour().isNeutral());
    shots.soundOptions().setVolume(akira::play::Volume(0.4));
    CHECK(std::abs(shots.streamSettings().volume().level() - 0.4) < 1e-9);
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

TEST(arisa_touch_drives_the_shell_on_every_pack)
{
    for (const provider::SurfaceSize size : { Shots::handheld, Shots::switch2Handheld })
    {
        for (const std::string_view pack : ThemePacks::names)
        {
            Shots shots;
            shots.model.ready();
            shots.showHouse();
            REQUIRE(shots.start(shots.shell(), pack, appearance::ColorSchemePreference::dark, size));
            CHECK(shots.viewportWidth() == 1280.0F);
            REQUIRE(shots.tap("Living room.primary"));
            CHECK(shots.consoleCommands().primaries == std::vector<std::string>{ "Living room" });
        }
        Shots settings;
        settings.model.ready();
        settings.showHouse();
        settings.model.showSettings(true);
        REQUIRE(settings.start(settings.shell(), ThemePacks::standard, appearance::ColorSchemePreference::dark, size));
        REQUIRE(settings.settingsTab(1));
        const std::optional<composition::Rect> before = settings.areaOf("general.accent");
        REQUIRE(before.has_value());
        REQUIRE(settings.drag("general.accent", 240.0F));
        const std::optional<composition::Rect> after = settings.areaOf("general.accent");
        REQUIRE(after.has_value());
        CHECK(after->y < before->y - 100.0F);
    }
    for (const std::string_view pack : ThemePacks::names)
    {
        Shots docked;
        docked.model.ready();
        docked.showHouse();
        REQUIRE(docked.start(docked.shell(), pack, appearance::ColorSchemePreference::dark, Shots::switch2Handheld,
            provider::DisplayMode::docked));
        CHECK(docked.viewportWidth() == 1280.0F);
    }
}

TEST(arisa_stream_dock_focus_moves_with_the_dock_buttons)
{
    Shots shots;
    REQUIRE(shots.start(Shots::video()));
    shots.streamWithDock();
    REQUIRE(shots.settle());
    CHECK(shots.focusedWithin("stream-dock.tabs"));
    const std::optional<composition::NodeId> start = shots.focused();
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::dpad_right }));
    const std::optional<composition::NodeId> right = shots.focused();
    CHECK(right.has_value() && right != start);
    CHECK(shots.focusedWithin("stream-dock.tabs"));
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = -1.0F, .y = 0.0F }));
    CHECK(shots.focused() == start);
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = 1.0F, .y = 0.0F }));
    CHECK(shots.focused() == right);
    REQUIRE(shots.pad(provider::Buttons{ provider::Button::dpad_up }));
    CHECK(shots.focusedWithin("stream-dock.panel"));
    REQUIRE(shots.pad(provider::Buttons{}, provider::Stick{ .x = 0.0F, .y = -1.0F }));
    CHECK(shots.focusedWithin("stream-dock.tabs"));
    CHECK(!shots.shows("stream-dock.akira/diagnostics/level"));
    REQUIRE(shots.tap(structure::DockBar::key_of(structure::DockBar::tab_key, 0)));
    CHECK(shots.shows("stream-dock.akira/diagnostics/level"));
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
    REQUIRE(shots.settingsTab(0));
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
    REQUIRE(shots.settingsTab(11));
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

TEST(arisa_shot_applet_notice)
{
    Shots shots;
    shots.model.restricted();
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("applet.message"));
    CHECK(!shots.shows("shell.page"));
    CHECK(shots.write("applet_notice"));
    REQUIRE(shots.press("applet.exit"));
    CHECK(shots.model.view().get().exitConfirmed);
}

TEST(arisa_shot_dns_warning)
{
    using namespace std::chrono_literals;
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    const auto opened = std::chrono::steady_clock::now();
    shots.model.warnDns("m.np.playstation.com", "192.168.20.1", opened);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("dns.ok"));
    CHECK(shots.write("dns_warning"));
    REQUIRE(shots.press("dns.ok"));
    CHECK(shots.model.view().get().dns.has_value());
    shots.model.tick(opened + 6s);
    REQUIRE(shots.settle());
    REQUIRE(shots.press("dns.ok"));
    CHECK(!shots.model.view().get().dns.has_value());
}

TEST(arisa_shot_file_logging_question)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    bool turnedOff = false;
    std::vector<akira::app::arisa::Confirmation::Choice> choices;
    choices.push_back(akira::app::arisa::Confirmation::Choice{
        .label = shots.text().text("akira/connection/file_logging_keep_on"), .chosen = [] {}, .key = "file-logging.keep" });
    choices.push_back(akira::app::arisa::Confirmation::Choice{ .label = shots.text().text("akira/connection/file_logging_turn_off"),
        .chosen = [&turnedOff] { turnedOff = true; },
        .emphasis = akira::app::arisa::Confirmation::Emphasis::Prominent,
        .key = "file-logging.turn-off" });
    REQUIRE(shots.start(shots.shell()));
    REQUIRE(shots.modal(akira::app::arisa::Confirmation(shots.text().text("akira/connection/file_logging_title"),
        shots.text().text("akira/connection/file_logging_warning"), std::move(choices), [] {})
            .dimmed("file-logging")));
    CHECK(shots.write("file_logging_question"));
    REQUIRE(shots.press("file-logging.turn-off"));
    CHECK(turnedOff);
}

TEST(arisa_shot_connecting_log_and_login_pin)
{
    Shots shots;
    shots.model.ready();
    shots.showHouse();
    shots.model.connecting("Living room");
    akira::play::ui::ConnectingScreen::Progress shown{ .console = "Living room", .stages = false };
    for (const char* line : { "21:40:12 Starting connection...", "21:40:12 [ARISA] starting stream to Living room",
             "21:40:12 [SESSION] requesting session at 192.168.20.40", "21:40:13 [SESSION] ctrl connected, auth ok",
             "21:40:13 [TAKION] negotiating protocol version 12", "21:40:13 [TAKION] measuring the connection",
             "21:40:14 [SESSION] the console asks for its login PIN" })
        shown.log.push_back(line);
    shots.model.stage(shown);
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("connecting.log"));
    CHECK(shots.write("connecting_log"));
    Shots pin;
    pin.model.ready();
    pin.showHouse();
    pin.model.connecting("Living room");
    pin.model.stage(shown);
    REQUIRE(pin.start(pin.shell()));
    REQUIRE(pin.modal(akira::play::ui::LoginPinCard(false, pin.streamControls(), pin.text())));
    REQUIRE(pin.press(structure::DigitKeypad::key_of('4')));
    REQUIRE(pin.press(structure::DigitKeypad::key_of('2')));
    CHECK(pin.write("login_pin"));
    Shots retry;
    retry.model.ready();
    retry.showHouse();
    retry.model.connecting("Living room");
    REQUIRE(retry.start(retry.shell()));
    REQUIRE(retry.modal(akira::play::ui::LoginPinCard(true, retry.streamControls(), retry.text())));
    CHECK(retry.shows("login-pin.error"));
    CHECK(retry.write("login_pin_retry"));
    REQUIRE(retry.press("login-pin.cancel"));
    CHECK(retry.streamControls().pinCancels == 1);
}

TEST(arisa_shot_fake_consoles)
{
    Shots shots;
    shots.model.ready();
    shots.showConsoles(akira::consoles::DebugConsoles::facts());
    REQUIRE(shots.start(shots.shell()));
    CHECK(shots.shows("PS5-Debug-1.primary"));
    CHECK(shots.write("consoles_fake"));
}
