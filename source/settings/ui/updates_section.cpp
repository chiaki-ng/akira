#include "settings/ui/updates_section.hpp"

#include <arisa/structure/form.hpp>

#include <string>
#include <utility>
#include <vector>

#include "app/app_settings.hpp"
#include "app/version.hpp"
#include "settings/developer_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::settings::ui {

namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;

UpdatesSection::UpdatesSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto UpdatesSection::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    const SettingRows rows(*model, *help, *strings);
    app::AppSettings& app = model->parts().app;
    SettingsModel* edit = model;
    std::vector<std::pair<std::string, std::string>> channels{ { "stable", rows.text("akira/update/channel_stable") } };
    if (model->parts().developer.powerUserMenuUnlocked())
        channels.emplace_back("rc", rows.text("akira/update/channel_rc"));
    std::string state;
    if (status.checking)
        state = rows.text("akira/update/checking");
    else if (status.update)
    {
        switch (status.update->outcome)
        {
            case Maintenance::UpdateCheck::Outcome::Failed: state = rows.text("akira/update/failed"); break;
            case Maintenance::UpdateCheck::Outcome::UpToDate: state = rows.text("akira/update/up_to_date"); break;
            case Maintenance::UpdateCheck::Outcome::Available: state = status.update->version; break;
        }
    }
    std::vector<composition::Element> shown;
    shown.push_back(rows.value(rows.text("akira/settings/version"),
        std::string("Akira ") + app::version::string() + " (" + app::version::channel() + ")"));
    shown.push_back(rows.picker<std::string>(rows.text("akira/update/channel"), "", std::move(channels),
        [&app] { return app.updateChannel() == "rc" ? std::string("rc") : std::string("stable"); },
        [&app](std::string chosen) { app.setUpdateChannel(chosen); }));
    shown.push_back(rows.toggle(rows.text("akira/update/auto_check"), "", [&app] { return app.autoCheckUpdates(); },
        [&app](bool value) { app.setAutoCheckUpdates(value); }));
    shown.push_back(rows.action(rows.text("akira/update/check_now"), "refresh", "", [edit] { edit->checkForUpdate(); })
                        .disabled(status.checking));
    if (!state.empty())
        shown.push_back(rows.note(state, false));
    return structure::Form(rows.section(rows.text("akira/settings/menu_updates"), std::move(shown)));
}

}
