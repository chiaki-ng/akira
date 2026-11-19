#pragma once

#include "app/services.hpp"

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
