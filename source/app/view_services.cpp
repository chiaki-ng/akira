#include "app/view_services.hpp"

#include <exception>

namespace akira::app {

ViewServices::ViewServices(Services& services)
{
    if (installed != nullptr)
        std::terminate();
    installed = &services;
}

ViewServices::~ViewServices()
{
    installed = nullptr;
}

Services& ViewServices::current()
{
    if (installed == nullptr)
        std::terminate();
    return *installed;
}

}
