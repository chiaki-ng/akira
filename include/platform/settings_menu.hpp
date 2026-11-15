#pragma once

#include <functional>
#include <string>

namespace brls
{
class Box;
}

namespace akira::platform
{

struct SettingsMenu {
    std::string title;
    std::function<brls::Box*()> create;
};

}
