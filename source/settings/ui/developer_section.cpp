#include "settings/ui/developer_section.hpp"

#include <arisa/structure/form.hpp>

#include <algorithm>
#include <utility>

#include "settings/developer_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::settings::ui {

namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;

DeveloperSection::DeveloperSection(SettingsModel& model, const HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto DeveloperSection::takion(TakionTarget target, const std::string& title, const std::vector<std::uint32_t>& implemented) const
    -> composition::Element
{
    const SettingRows rows(*model, *help, *strings);
    DeveloperSettings& developer = model->parts().developer;
    const int current = developer.takionVersion(target);
    std::vector<std::pair<int, std::string>> choices{ { 0, rows.text("akira/settings/dev_takion_version_auto") } };
    for (const std::uint32_t version : implemented)
        choices.emplace_back(static_cast<int>(version), "v" + std::to_string(version));
    if (current != 0 && std::ranges::none_of(choices, [current](const auto& choice) { return choice.first == current; }))
        choices.emplace_back(current, "v" + std::to_string(current));
    return rows.picker<int>(title, "settings/devTakionVersion", std::move(choices),
        [&developer, target] { return developer.takionVersion(target); },
        [&developer, target](int version) { developer.setTakionVersion(target, version); });
}

auto DeveloperSection::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    const SettingRows rows(*model, *help, *strings);
    const DeviceFeatures& features = model->features();
    DeveloperSettings& developer = model->parts().developer;
    Maintenance& maintenance = model->parts().maintenance;
    SettingsModel* edit = model;
    std::vector<composition::Element> versions;
    versions.push_back(takion(TakionTarget::PS4, rows.text("akira/settings/dev_takion_version_ps4"), features.takionPs4));
    versions.push_back(takion(TakionTarget::PS5, rows.text("akira/settings/dev_takion_version_ps5"), features.takionPs5));
    if (features.cloud)
    {
        versions.push_back(takion(TakionTarget::PS5Cloud, rows.text("akira/settings/dev_takion_version_ps5_cloud"),
            features.takionPs5Cloud));
        versions.push_back(takion(TakionTarget::PSNow, rows.text("akira/settings/dev_takion_version_ps_now"), features.takionPsNow));
    }
    std::vector<composition::Element> testing;
    testing.push_back(rows.toggle(rows.text("akira/settings/dev_fake_hosts"), "settings/devFakeHosts",
        [&developer] { return developer.fakeHosts(); },
        [&developer, &maintenance](bool value) {
            developer.setFakeHosts(value);
            maintenance.consolesChanged();
        }));
    testing.push_back(rows.toggle(rows.text("akira/settings/dev_loading_preview"), "settings/devLoadingPreview",
        [&developer] { return developer.loadingPreview(); }, [&developer](bool value) { developer.setLoadingPreview(value); }));
    if (features.selfUpdate)
        testing.push_back(rows.action(rows.text("akira/settings/simulate_update"), "update", "settings/simulateUpdate",
            [edit] { edit->ask(SettingsModel::Entry::UpdateServer); }));
    testing.push_back(rows.action(rows.text("akira/settings/dev_tls_verify_probe"), "shield", "settings/tlsVerifyProbe",
        [edit] { edit->probeTls(); }));
    if (!status.tls.empty())
        testing.push_back(rows.note(status.tls, status.tls.starts_with("FAIL")));
    testing.push_back(rows.link(rows.text("akira/settings/dev_logs"), features.logFolder.string(), "settings/devLogs",
        [edit] { edit->open(SettingsModel::Detail::Logs); }));
    return structure::Form(rows.section(rows.text("akira/settings/takion_heading"), std::move(versions)),
        rows.section(rows.text("akira/settings/testing_heading"), std::move(testing)));
}

}
