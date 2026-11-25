#include "settings/ui/controls_section.hpp"

#include <arisa/structure/form.hpp>

#include <utility>
#include <vector>

#include "app/app_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::settings::ui {

namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;

ControlsSection::ControlsSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto ControlsSection::summaryKey(PadBench::Summary summary) -> std::string
{
    switch (summary)
    {
        case PadBench::Summary::Basic: return "akira/settings/rumble_output_basic";
        case PadBench::Summary::Native: return "akira/settings/rumble_output_native";
        case PadBench::Summary::SwitchProfile: return "akira/settings/rumble_tier_switch";
        case PadBench::Summary::DefaultProfile: return "akira/settings/rumble_tier_default";
        case PadBench::Summary::ThisController: return "akira/settings/rumble_tier_unit";
        case PadBench::Summary::Other: break;
    }
    return {};
}

auto ControlsSection::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    const SettingRows rows(*model, *help, *strings);
    SettingsModel* edit = model;
    app::AppSettings& app = model->parts().app;
    std::vector<composition::Element> pads;
    for (std::size_t index = 0; index < status.pads.size(); ++index)
    {
        const PadBench::Pad& pad = status.pads[index];
        const std::string key = summaryKey(pad.summary);
        pads.push_back(rows.link(pad.label, key.empty() ? pad.key : rows.text(key), "",
            [edit, index] { edit->open(SettingsModel::Detail::Pad, index); }));
    }
    if (pads.empty())
        pads.push_back(rows.value(rows.text("akira/settings/connected_controllers"),
            rows.text("akira/settings/connected_controllers_none")));
    return structure::Form(
        rows.section(rows.text("akira/settings/session_heading"),
            rows.toggle(rows.text("akira/settings/sleep_on_exit"), "settings/sleepOnExit",
                [&app] { return app.sleepOnExit(); }, [&app](bool value) { app.setSleepOnExit(value); })),
        rows.section(rows.text("akira/settings/buttons_heading"),
            rows.link(rows.text("akira/settings/button_mapping"), rows.text("akira/common/configure"),
                "settings/buttonMapping", [edit] { edit->open(SettingsModel::Detail::Mapping); })),
        rows.section(rows.text("akira/settings/connected_controllers"), std::move(pads)))
        .on_appear([edit] { edit->scanPads(); });
}

}
