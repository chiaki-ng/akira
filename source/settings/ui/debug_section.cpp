#include "settings/ui/debug_section.hpp"

#include <arisa/structure/form.hpp>

#include <vector>

#include "settings/chiaki_log_verbosity.hpp"
#include "settings/developer_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::settings::ui {

namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;

DebugSection::DebugSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto DebugSection::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(model->view()));
    const SettingRows rows(*model, *help, *strings);
    DeveloperSettings& developer = model->parts().developer;
    auto flag = [&rows, &developer](const char* key, const char* id, bool (DeveloperSettings::*read)() const,
                    void (DeveloperSettings::*write)(bool)) {
        return rows.toggle(rows.text(key), id, [&developer, read] { return (developer.*read)(); },
            [&developer, write](bool value) { (developer.*write)(value); });
    };
    std::vector<composition::Element> shown;
    shown.push_back(flag("akira/settings/silence_all_logs", "settings/silenceAllLogs", &DeveloperSettings::silenceAllLogs,
        &DeveloperSettings::setSilenceAllLogs));
    shown.push_back(flag("akira/settings/file_logging", "settings/enableFileLogging", &DeveloperSettings::fileLogging,
        &DeveloperSettings::setFileLogging));
    shown.push_back(flag("akira/settings/lwip_log", "settings/debugLwipLog", &DeveloperSettings::lwipLog,
        &DeveloperSettings::setLwipLog));
    shown.push_back(flag("akira/settings/wireguard_log", "settings/debugWireguardLog", &DeveloperSettings::wireguardLog,
        &DeveloperSettings::setWireguardLog));
    shown.push_back(flag("akira/settings/render_log", "settings/debugRenderLog", &DeveloperSettings::renderLog,
        &DeveloperSettings::setRenderLog));
    shown.push_back(rows.picker<ChiakiLogVerbosity>(rows.text("akira/settings/chiaki_log"), "settings/debugChiakiLog",
        { { ChiakiLogVerbosity::Normal, rows.text("akira/settings/chiaki_log_normal") },
            { ChiakiLogVerbosity::Info, rows.text("akira/settings/chiaki_log_info") },
            { ChiakiLogVerbosity::Debug, rows.text("akira/settings/chiaki_log_debug") },
            { ChiakiLogVerbosity::Trace, rows.text("akira/settings/chiaki_log_trace") } },
        [&developer] { return developer.chiakiLogVerbosity(); },
        [&developer](ChiakiLogVerbosity chosen) { developer.setChiakiLogVerbosity(chosen); }));
    shown.push_back(flag("akira/settings/discovery_log", "settings/debugDiscoveryLog", &DeveloperSettings::discoveryLog,
        &DeveloperSettings::setDiscoveryLog));
    shown.push_back(flag("akira/settings/ffmpeg_log", "settings/debugFfmpegLog", &DeveloperSettings::ffmpegLog,
        &DeveloperSettings::setFfmpegLog));
    return structure::Form(rows.section(rows.text("akira/settings/debug_logging"), std::move(shown)));
}

}
