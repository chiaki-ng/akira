#include "app/arisa/shell_model.hpp"

#include <utility>

namespace akira::app::arisa {

void ShellModel::ready()
{
    if (state.get().phase == Phase::Starting)
        state.write(&View::phase, Phase::Home);
}

void ShellModel::connecting(std::string console)
{
    state.update([&console](View& view) {
        view.phase = Phase::Connecting;
        view.progress = play::ui::ConnectingScreen::Progress{ .console = std::move(console) };
        view.ending.reset();
        view.settings = false;
    });
}

void ShellModel::stage(play::ui::ConnectingScreen::Progress progress)
{
    state.write(&View::progress, std::move(progress));
}

void ShellModel::streaming()
{
    state.write(&View::phase, Phase::Streaming);
}

void ShellModel::home()
{
    state.write(&View::phase, Phase::Home);
}

void ShellModel::ended(Ending ending)
{
    state.update([&ending](View& view) {
        view.phase = Phase::Home;
        view.ending = std::move(ending);
    });
}

void ShellModel::dismissEnding()
{
    state.write(&View::ending, std::optional<Ending>());
}

void ShellModel::showSettings(bool shown)
{
    state.write(&View::settings, shown);
}

}
