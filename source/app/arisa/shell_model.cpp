#include "app/arisa/shell_model.hpp"

#include <utility>

#include "network/sony_dns_checker.hpp"

namespace akira::app::arisa {

void ShellModel::ready()
{
    if (state.get().phase == Phase::Starting)
        state.write(&View::phase, Phase::Home);
}

void ShellModel::restricted()
{
    if (state.get().phase == Phase::Starting)
        state.write(&View::phase, Phase::Restricted);
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

void ShellModel::show(Page page)
{
    state.update([page](View& view) {
        view.settings = false;
        if ((page == Page::Cloud && !view.cloudOffered) || (page == Page::Trophies && !view.trophiesOffered))
            return;
        view.page = page;
    });
}

void ShellModel::offer(bool cloud, bool trophies)
{
    state.update([cloud, trophies](View& view) {
        view.cloudOffered = cloud;
        view.trophiesOffered = trophies;
        if ((view.page == Page::Cloud && !cloud) || (view.page == Page::Trophies && !trophies))
            view.page = Page::Consoles;
    });
}

void ShellModel::askExit()
{
    state.update([](View& view) {
        if (view.phase == Phase::Home && !view.ending)
            view.exiting = true;
    });
}

void ShellModel::dismissExit()
{
    state.write(&View::exiting, false);
}

void ShellModel::confirmExit()
{
    state.update([](View& view) {
        view.exiting = false;
        view.exitConfirmed = true;
    });
}

void ShellModel::warnDns(std::string domain, std::string server, std::chrono::steady_clock::time_point now)
{
    dnsShown = now;
    state.write(&View::dns,
        std::optional<DnsWarning>(DnsWarning{
            .domain = std::move(domain), .server = std::move(server), .wait = network::secondsUntilDnsWarningOk(0) }));
}

void ShellModel::tick(std::chrono::steady_clock::time_point now)
{
    const std::optional<DnsWarning>& shown = state.get().dns;
    if (!shown || !dnsShown)
        return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - *dnsShown);
    const int wait = network::secondsUntilDnsWarningOk(elapsed.count());
    if (wait != shown->wait)
        state.update([wait](View& view) { view.dns->wait = wait; });
}

void ShellModel::dismissDns()
{
    if (!state.get().dns || state.get().dns->wait > 0)
        return;
    dnsShown.reset();
    state.write(&View::dns, std::optional<DnsWarning>());
}

}
