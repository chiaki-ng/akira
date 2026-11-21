#pragma once

#include <string>

#include "consoles/ui/console_shelf.hpp"

namespace akira::consoles::ui {

class ConsoleCommands {
public:
    virtual ~ConsoleCommands() = default;

    virtual void primary(const std::string& catalogName) = 0;
    virtual void registerOverInternet(const std::string& catalogName) = 0;
    virtual void askPin(const std::string& catalogName) = 0;
    virtual void registerWithPin(const std::string& catalogName, const std::string& pin) = 0;
    virtual void cancelRegistration(const std::string& catalogName) = 0;
    virtual void link(const std::string& catalogName, const std::string& localName) = 0;
    virtual void edit(const std::string& catalogName) = 0;
    virtual void saveEdit(const std::string& catalogName, const ConsoleShelf::Edit& values) = 0;
    virtual void askDelete(const std::string& catalogName) = 0;
    virtual void remove(const std::string& catalogName) = 0;
    virtual void openAdd() = 0;
    virtual void chooseAddMode(ConsoleShelf::AddMode mode) = 0;
    virtual void addByAddress(const std::string& address, bool ps5) = 0;
    virtual void findRemote() = 0;
    virtual void setUpAccount() = 0;
    virtual void launchTitle(const std::string& catalogName, const std::string& titleId) = 0;
};

}
