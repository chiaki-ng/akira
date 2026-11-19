#ifndef AKIRA_UPDATE_FLOW_HPP
#define AKIRA_UPDATE_FLOW_HPP

#include "platform/updates/github/update_manager.hpp"

class HttpPool;

namespace akira::settings
{
class DeveloperSettings;
class SettingsStore;
}

namespace akira::platform::github
{

class UpdateFlow {
public:
    struct Services {
        UpdateManager& manager;
        HttpPool& http;
        app::AppSettings& app;
        settings::DeveloperSettings& developer;
        settings::SettingsStore& store;
    };

    explicit UpdateFlow(Services services);

    void checkOnLaunch() const;
    void promptUpdate(const UpdateInfo& info) const;
    void simulate() const;

private:
    static void showUpdateError(const std::string& message);

    void startDownload(const UpdateInfo& info) const;

    Services services;
};

}

#endif // AKIRA_UPDATE_FLOW_HPP
