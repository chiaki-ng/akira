#pragma once

#include <stb_image_write.h>

#include <arisa/composition/element.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/introspection.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "app/arisa/art_session.hpp"
#include "app/arisa/notices.hpp"
#include "app/arisa/shell_model.hpp"
#include "app/arisa/shell_screen.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/theme_packs.hpp"
#include "app/notifier.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "fixture_art.hpp"
#include "play/ui/stream_commands.hpp"

namespace tests {

class QuietConsoleCommands final : public akira::consoles::ui::ConsoleCommands {
public:
    void primary(const std::string& name) override { played.push_back(name); }
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
    void launchTitle(const std::string& name, const std::string& titleId) override
    {
        launched.emplace_back(name, titleId);
    }

    std::vector<std::string> played;
    std::vector<std::pair<std::string, std::string>> launched;
};

class QuietStreamCommands final : public akira::play::ui::StreamCommands {
public:
    void cancel() override {}
    void reconnect() override {}
    void restart() override {}
    void resume() override {}
    void leave(bool) override {}
    void goHome() override {}
    void resetMotion() override {}
    void tuneColour(akira::play::ColourAdjustments) override {}
    void tuneVolume(akira::play::Volume) override {}
    void showDiagnostics(akira::play::DiagnosticsAppearance) override {}
    auto diagnostics() -> akira::play::DiagnosticsReport override { return {}; }
};

class CollectedNotices final : public akira::app::Notifier {
public:
    void notify(std::string text) override { shown.push_back(std::move(text)); }

    std::vector<std::string> shown;
};

class ScreenShots {
public:
    static constexpr int settleFrames = 90;
    static constexpr int artRounds = 400;
    static constexpr int quietRounds = 20;
    static constexpr std::chrono::milliseconds artPause{ 5 };
    static constexpr std::chrono::milliseconds frameStep{ 16 };
    static constexpr arisa::provider::SurfaceSize handheld{ .width = arisa::provider::SurfaceWidth{ 1280 },
        .height = arisa::provider::SurfaceHeight{ 720 } };

    ScreenShots()
        : packs(AKIRA_RESOURCES_DIR)
        , art(std::filesystem::path(AKIRA_RESOURCES_DIR) / ".." / "tests" / "arisa" / "fixtures" / "art")
        , session(art)
    {
        loadAssets();
        strings.emplace(providers.assets(), akira::app::arisa::StringCatalog::fallbackLocale);
        model.ready();
    }

    ScreenShots(const ScreenShots&) = delete;
    ScreenShots& operator=(const ScreenShots&) = delete;

    auto text() const -> const akira::app::arisa::StringCatalog& { return *strings; }
    auto artSession() const -> const akira::app::arisa::ArtSession& { return session; }
    auto fixtures() -> FixtureArt& { return art; }

    auto parts() -> akira::app::arisa::ShellScreen::Parts
    {
        akira::app::arisa::ShellScreen::Parts made{};
        made.shelf = &shelf;
        made.consoles = &consoles;
        made.stream = &stream;
        made.art = &session;
        return made;
    }

    auto shell(const akira::app::arisa::ShellScreen::Parts& shown) -> arisa::composition::Element
    {
        held = shown;
        return akira::app::arisa::ShellScreen(model, notices, held, *strings);
    }

    auto start(arisa::composition::Element screen) -> bool
    {
        providers.surface().set_display_mode(arisa::provider::DisplayMode::handheld);
        providers.surface().resize(handheld);
        providers.gpu().rasterize(handheld);
        arisa::runtime::AppearanceOptions look = packs.options(akira::app::arisa::ThemePacks::standard);
        look.policy.defaults.color_scheme = arisa::appearance::ColorSchemePreference::dark;
        auto created = arisa::runtime::Application::create(providers,
            std::move(screen).async_image_url_session(session.session()),
            arisa::runtime::ApplicationOptions{ .appearance = std::move(look) });
        if (!created)
            return false;
        application.emplace(std::move(*created));
        return settle() && settleArt();
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

    auto settleArt() -> bool
    {
        std::size_t seen = art.requests();
        int quiet = 0;
        for (int round = 0; round < artRounds && quiet < quietRounds; ++round)
        {
            if (!settle(4))
                return false;
            std::this_thread::sleep_for(artPause);
            const std::size_t asked = art.requests();
            quiet = art.settled() && asked == seen ? quiet + 1 : 0;
            seen = asked;
        }
        return settle(12) && art.settled();
    }

    auto press(std::string_view key) -> bool
    {
        return application->root().activate(arisa::composition::ElementKey{ std::string(key) }) && settle()
            && settleArt();
    }

    auto pad(arisa::provider::Buttons buttons) -> bool
    {
        arisa::provider::InputFrame down;
        down.pads[0].connected = true;
        down.pads[0].buttons = buttons;
        providers.input().push(down);
        if (!settle(6))
            return false;
        arisa::provider::InputFrame up;
        up.pads[0].connected = true;
        providers.input().push(up);
        return settle(12) && settleArt();
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

    auto appearance() const -> const arisa::appearance::AppearanceModel& { return *application->appearance(); }

    auto focusedWithin(std::string_view key) const -> bool
    {
        std::optional<arisa::composition::NodeId> at = application->introspection().focus().focused;
        while (at)
        {
            const std::optional<arisa::runtime::Introspection::NodeRecord> record = application->introspection().node(*at);
            if (!record)
                return false;
            if (record->key == key)
                return true;
            at = record->parent;
        }
        return false;
    }

    auto has(std::string_view key) const -> bool
    {
        return std::ranges::any_of(application->introspection().nodes(),
            [key](const arisa::runtime::Introspection::NodeRecord& record) { return record.key == key; });
    }

    akira::app::arisa::ShellModel model;
    akira::app::arisa::Notices notices;
    akira::consoles::ui::ConsoleShelf shelf;
    QuietConsoleCommands consoles;
    QuietStreamCommands stream;
    CollectedNotices collected;

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
        for (const char* grade : { "platinum", "gold", "silver", "bronze" })
            add(std::string("img/trophy/") + grade + ".png", resources / "img" / "trophy" / (std::string(grade) + ".png"));
        add("strings/en.toml", resources / "strings" / "en.toml");
    }

    void add(std::string_view at, const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            return;
        const std::vector<char> raw{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        std::vector<std::byte> bytes(raw.size());
        std::ranges::transform(raw, bytes.begin(), [](char value) { return std::byte(value); });
        if (auto path = arisa::provider::AssetPath::create(at))
            providers.assets().add(std::move(*path), bytes);
    }

    arisa::provider::headless::HeadlessProviderFactory providers;
    std::optional<akira::app::arisa::StringCatalog> strings;
    akira::app::arisa::ThemePacks packs;
    akira::app::arisa::ShellScreen::Parts held{};
    std::optional<arisa::runtime::Application> application;
    FixtureArt art;
    akira::app::arisa::ArtSession session;
};

}
