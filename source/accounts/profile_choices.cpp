#include "accounts/profile_choices.hpp"

#include "app/app_settings.hpp"

namespace akira::accounts {

ProfileChoices::ProfileChoices(const std::vector<Profile>& profiles, int64_t activeId, const app::AppSettings& app)
{
    for (std::size_t i = 0; i < profiles.size(); i++) {
        const std::string label = profiles[i].label();
        entries.push_back(Choice{
            .label = label.empty() ? std::nullopt : std::optional<std::string>(app.maskAccountName(label)),
            .id = profiles[i].id,
        });
        if (profiles[i].id == activeId)
            active = static_cast<int>(i);
    }
}

std::optional<int64_t> ProfileChoices::idAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(entries.size()))
        return std::nullopt;
    return entries[static_cast<std::size_t>(index)].id;
}

}
