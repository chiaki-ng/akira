#ifndef AKIRA_UPDATE_FLOW_HPP
#define AKIRA_UPDATE_FLOW_HPP

#include <string>

#include "app/services.hpp"
#include "platform/updates/update_info.hpp"

namespace akira::app::views
{

class UpdateFlow {
public:
    explicit UpdateFlow(Services& services);

    void checkOnLaunch() const;
    void promptUpdate(const platform::UpdateInfo& info) const;
    void simulate() const;

private:
    static void showUpdateError(const std::string& message);

    void startDownload(const platform::UpdateInfo& info) const;

    Services& services;
};

}

#endif // AKIRA_UPDATE_FLOW_HPP
