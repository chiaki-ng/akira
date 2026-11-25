#pragma once

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/services.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/console_settings.hpp"
#include "consoles/host_catalog.hpp"
#include "network/network_settings.hpp"
#include "play/controller_settings.hpp"
#include "play/stream_settings.hpp"
#include "play/stream_state.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::app {

class ViewServices {
public:
    explicit ViewServices(Services& services);
    ~ViewServices();

    ViewServices(const ViewServices&) = delete;
    ViewServices& operator=(const ViewServices&) = delete;

    static Services& current();

private:
    static inline Services* installed = nullptr;
};

}
