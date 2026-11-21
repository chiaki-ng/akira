#include "test_util.hpp"

#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "consoles/console_list.hpp"
#include "consoles/ui/console_commands.hpp"
#include "consoles/ui/console_grid.hpp"
#include "consoles/ui/console_shelf.hpp"
#include "fake_strings.hpp"
#include "play/connect_flow.hpp"
#include "play/ui/connecting_screen.hpp"
#include "play/ui/stream_commands.hpp"

namespace {

namespace components = arisa::components;
namespace composition = arisa::composition;

using akira::consoles::ConsoleEntry;
using akira::consoles::ConsoleFacts;
using akira::consoles::ConsoleList;
using akira::consoles::HostType;
using akira::consoles::ui::ConsoleCommands;
using akira::consoles::ui::ConsoleGrid;
using akira::consoles::ui::ConsoleShelf;
using akira::play::ConnectFlow;
using akira::play::ui::ConnectingScreen;

class RecordingConsoles final : public ConsoleCommands {
public:
    void primary(const std::string& catalogName) override { played.push_back(catalogName); }
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
    void setUpAccount() override { ++setUps; }
    void launchTitle(const std::string&, const std::string&) override {}

    std::vector<std::string> played;
    int setUps = 0;
};

class RecordingStream final : public akira::play::ui::StreamCommands {
public:
    void cancel() override { ++cancelled; }
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

    int cancelled = 0;
};

class ProgressHost final : public composition::Component {
public:
    ProgressHost(const composition::Observable<ConnectingScreen::Progress>& progress, RecordingStream& stream,
        const akira::app::Strings& strings)
        : progress(&progress)
        , stream(&stream)
        , strings(&strings)
    {
    }

    bool operator==(const ProgressHost&) const = default;

    auto body(composition::Context& context) const -> composition::Element override
    {
        return ConnectingScreen(context.read(*progress), *stream, *strings);
    }

private:
    const composition::Observable<ConnectingScreen::Progress>* progress;
    RecordingStream* stream;
    const akira::app::Strings* strings;
};

auto facts(std::string name, bool paired, bool discovered) -> ConsoleFacts
{
    ConsoleFacts made;
    made.catalogName = name;
    made.name = std::move(name);
    made.type = HostType::Discovered;
    made.ps5 = true;
    made.paired = paired;
    made.discovered = discovered;
    made.ready = true;
    made.hasAddress = true;
    made.presence = "Ready";
    return made;
}

auto shows(const composition::Node& node, std::string_view wanted) -> bool
{
    if (node.phase() == composition::NodeLifecyclePhase::departing)
        return false;
    if (const auto text = node.content_as<components::Text::Content>(); text && text->get().text() == wanted)
        return true;
    for (const composition::Node& child : node.children())
        if (shows(child, wanted))
            return true;
    return false;
}

auto has(arisa::runtime::Application& application, std::string_view key) -> bool
{
    return application.root().tree().find(composition::ElementKey{ std::string(key) }).has_value();
}

auto strings() -> tests::FakeStrings
{
    return tests::FakeStrings({
        { "akira/tabs/consoles", "Consoles" },
        { "akira/hosts/play", "Play" },
        { "akira/hosts/register", "Register" },
        { "akira/connection/starting_stream", "Starting your stream" },
        { "akira/connection/step_of", "Step {} of {}" },
        { "akira/connection/stage_finding", "Finding your console" },
        { "akira/connection/stage_authenticating", "Authenticating" },
        { "akira/connection/stage_tuning", "Tuning the connection" },
        { "akira/connection/stage_starting", "Starting the stream" },
    });
}

}

TEST(arisa_console_grid_columns_follow_chiaki_native)
{
    CHECK(ConsoleGrid::columnsFor(1280.0F, 3) == 2);
    CHECK(ConsoleGrid::columnsFor(1280.0F, 1) == 1);
    CHECK(ConsoleGrid::columnsFor(900.0F, 3) == 1);
}

TEST(arisa_console_grid_shows_the_console_list_and_follows_discovery)
{
    arisa::provider::headless::HeadlessProviderFactory providers;
    const tests::FakeStrings text = strings();
    ConsoleShelf shelf;
    RecordingConsoles commands;
    shelf.show(ConsoleList({ facts("Living room", true, true), facts("Study", false, true) }, ConsoleList::Profile::Signed));
    auto application = arisa::runtime::Application::create(providers, ConsoleGrid(shelf, commands, text));
    REQUIRE(application.has_value());
    REQUIRE(application->frame().has_value());
    REQUIRE(application->frame().has_value());

    const composition::Node& root = application->root().tree().root();
    CHECK(shows(root, "Living room"));
    CHECK(!shows(root, "Study"));
    CHECK(has(*application, "Living room.primary"));
    CHECK(!has(*application, "Study.primary"));
    REQUIRE(shelf.view().get().nearby.size() == 1);
    CHECK(shelf.view().get().nearby.front().catalogName == "Study");
    CHECK(has(*application, "Living room.route"));
    CHECK(!has(*application, "consoles.add"));

    shelf.show(ConsoleList({ facts("Living room", true, true), facts("Study", false, true), facts("Bedroom", true, true) },
        ConsoleList::Profile::Signed));
    REQUIRE(application->frame().has_value());
    CHECK(shows(application->root().tree().root(), "Bedroom"));

    CHECK(application->root().activate(composition::ElementKey{ "Living room.primary" }));
    REQUIRE(application->frame().has_value());
    REQUIRE(commands.played.size() == 1);
    CHECK(commands.played.front() == "Living room");
}

TEST(arisa_console_grid_shows_the_empty_and_signed_out_states)
{
    arisa::provider::headless::HeadlessProviderFactory providers;
    const tests::FakeStrings text = strings();
    ConsoleShelf shelf;
    RecordingConsoles commands;
    shelf.show(ConsoleList({ facts("Living room", true, true) }, ConsoleList::Profile::None));
    auto application = arisa::runtime::Application::create(providers, ConsoleGrid(shelf, commands, text));
    REQUIRE(application.has_value());
    REQUIRE(application->frame().has_value());
    CHECK(has(*application, "consoles.no-profile"));
    CHECK(!shows(application->root().tree().root(), "Living room"));

    CHECK(has(*application, "consoles.set-up"));
    CHECK(application->root().activate(composition::ElementKey{ "consoles.set-up" }));
    REQUIRE(application->frame().has_value());
    CHECK(commands.setUps == 1);

    shelf.show(ConsoleList({}, ConsoleList::Profile::Signed));
    REQUIRE(application->frame().has_value());
    CHECK(has(*application, "consoles.empty"));
    CHECK(has(*application, "consoles.add"));
}

TEST(arisa_connecting_screen_steps_through_the_connect_flow)
{
    using libchiaking::play::ConnectStep;
    arisa::provider::headless::HeadlessProviderFactory providers;
    const tests::FakeStrings text = strings();
    RecordingStream stream;
    ConnectFlow flow;
    composition::Observable<ConnectingScreen::Progress> progress(
        ConnectingScreen::progressOf(flow.ring(), "Living room", text));
    auto application = arisa::runtime::Application::create(providers, ProgressHost(progress, stream, text));
    REQUIRE(application.has_value());
    REQUIRE(application->frame().has_value());
    CHECK(shows(application->root().tree().root(), "Starting your stream"));
    CHECK(has(*application, "connecting.progress"));

    struct Expected {
        ConnectStep step;
        const char* status;
        int active;
    };
    const std::vector<Expected> stages{
        { ConnectStep::finding_console, "Finding your console", 1 },
        { ConnectStep::requesting_session, "Authenticating", 3 },
        { ConnectStep::measuring_connection, "Tuning the connection", 4 },
        { ConnectStep::starting_stream, "Starting the stream", 5 },
    };
    for (const Expected& expected : stages)
    {
        flow.follow(expected.step, false);
        const ConnectingScreen::Progress shown = ConnectingScreen::progressOf(flow.ring(), "Living room", text);
        progress.set(shown);
        REQUIRE(application->frame().has_value());
        CHECK(shows(application->root().tree().root(), expected.status));
        CHECK(ConnectingScreen::capsule(expected.active, shown.step) == ConnectingScreen::Capsule::Active);
        CHECK(ConnectingScreen::capsule(expected.active - 1, shown.step) == ConnectingScreen::Capsule::Done);
        CHECK(has(*application, "connecting.step." + std::to_string(shown.steps - 1)));
    }

    CHECK(application->root().activate(composition::ElementKey{ "connecting.cancel" }));
    REQUIRE(application->frame().has_value());
    CHECK(stream.cancelled == 1);
}
