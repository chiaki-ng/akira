#include "settings/ui/power_user_section.hpp"

#include <arisa/structure/form.hpp>

#include <cmath>
#include <format>
#include <vector>

#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "network/network_settings.hpp"
#include "play/stream_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::settings::ui {

namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;

PowerUserSection::PowerUserSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto PowerUserSection::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(model->view()));
    const SettingRows rows(*model, *help, *strings);
    const SettingsModel::Parts& parts = model->parts();
    play::StreamSettings& stream = parts.stream;
    network::NetworkSettings& network = parts.network;
    DeveloperSettings& developer = parts.developer;
    app::AppSettings& app = parts.app;
    Maintenance& maintenance = parts.maintenance;
    std::vector<composition::Element> streaming;
    streaming.push_back(rows.toggle(rows.text("akira/settings/unlock_bitrate_max"), "settings/unlockBitrateMax",
        [&stream] { return stream.unlockBitrateMax(); }, [&stream](bool value) { stream.setUnlockBitrateMax(value); }));
    streaming.push_back(rows.toggle(rows.text("akira/settings/auto_reconnect"), "settings/autoReconnect",
        [&network] { return network.autoReconnect(); }, [&network](bool value) { network.setAutoReconnect(value); }));
    streaming.push_back(rows.toggle(rows.text("akira/settings/hide_account_name"), "settings/hideAccountName",
        [&app] { return app.hideAccountName(); },
        [&app, &maintenance](bool value) {
            app.setHideAccountName(value);
            maintenance.accountNameDisplayChanged();
        }));
    if (model->features().statsExport)
        streaming.push_back(rows.toggle(rows.text("akira/settings/ipc_stats"), "settings/ipcStats",
            [&developer] { return developer.ipcStatsEnabled(); },
            [&developer](bool value) { developer.setIpcStatsEnabled(value); }));
    std::vector<composition::Element> holepunch;
    holepunch.push_back(rows.toggle(rows.text("akira/settings/port_guessing"), "settings/portGuessing",
        [&network] { return network.portGuessing(); }, [&network](bool value) { network.setPortGuessing(value); }));
    holepunch.push_back(rows.slider(rows.text("akira/settings/port_guess_count"), "settings/portGuessingCount",
        controls::ClosedRange<double>{ .lower_bound = 1.0, .upper_bound = 75.0 }, 1.0,
        [&network] { return static_cast<double>(network.portGuessingCount()); },
        [&network](double value) { network.setPortGuessingCount(static_cast<int>(std::lround(value))); },
        std::format("{}", network.portGuessingCount())));
    holepunch.push_back(rows.slider(rows.text("akira/settings/probe_socket_count"), "settings/portGuessingSocks",
        controls::ClosedRange<double>{ .lower_bound = 1.0, .upper_bound = 250.0 }, 1.0,
        [&network] { return static_cast<double>(network.portGuessingSocks()); },
        [&network](double value) { network.setPortGuessingSocks(static_cast<int>(std::lround(value))); },
        std::format("{}", network.portGuessingSocks())));
    app::Notifier& notifier = parts.notifier;
    std::string flushed = rows.text("akira/settings/trophy_cache_flushed");
    return structure::Form(rows.section(rows.text("akira/settings/streaming_heading"), std::move(streaming)),
        rows.section(rows.text("akira/settings/holepunch_heading"), std::move(holepunch)),
        rows.section(rows.text("akira/settings/tools_heading"),
            rows.action(rows.text("akira/settings/flush_trophy_cache"), "delete", "settings/flushTrophyCache",
                [&maintenance, &notifier, flushed] {
                    maintenance.flushTrophyCache();
                    notifier.notify(flushed);
                })));
}

}
