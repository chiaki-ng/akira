#include "app/arisa/shell_model.hpp"

#include <algorithm>
#include <utility>

namespace akira::app::arisa {

void ShellModel::list(const std::vector<consoles::ConsoleFacts>& facts)
{
    std::vector<Console> shown;
    for (const consoles::ConsoleFacts& fact : facts)
    {
        if (fact.cloud)
            continue;
        shown.push_back(Console{ .catalogName = fact.catalogName, .name = fact.name, .ready = fact.ready,
            .paired = fact.paired });
    }
    std::ranges::sort(shown, {}, &Console::name);
    state.write(&View::consoles, std::move(shown));
}

void ShellModel::connecting(std::string target)
{
    state.update([&target](View& view) {
        view.phase = Phase::Connecting;
        view.target = std::move(target);
        view.status.clear();
    });
}

void ShellModel::stage(std::string label)
{
    state.write(&View::status, std::move(label));
}

void ShellModel::streaming()
{
    state.write(&View::phase, Phase::Streaming);
}

void ShellModel::stopped(std::string status)
{
    state.update([&status](View& view) {
        view.phase = Phase::Consoles;
        view.target.clear();
        view.status = std::move(status);
    });
}

}
