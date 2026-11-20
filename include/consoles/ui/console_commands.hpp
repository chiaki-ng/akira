#pragma once

#include <string>

#include "consoles/console_entry.hpp"

namespace akira::consoles::ui {

class ConsoleCommands {
public:
    virtual ~ConsoleCommands() = default;

    virtual void primary(const std::string& catalogName) = 0;
    virtual void option(const std::string& catalogName, ConsoleEntry::Option option) = 0;
    virtual void wake(const std::string& catalogName) = 0;
    virtual void registerOverInternet(const std::string& catalogName) = 0;
    virtual void askPin(const std::string& catalogName) = 0;
    virtual void registerWithPin(const std::string& catalogName, const std::string& pin) = 0;
    virtual void cancelRegistration(const std::string& catalogName) = 0;
    virtual void link(const std::string& catalogName, const std::string& localName) = 0;
    virtual void setConsolePin(const std::string& catalogName, const std::string& pin) = 0;
    virtual void setAddress(const std::string& catalogName, const std::string& address) = 0;
    virtual void pinTakion(const std::string& catalogName, int choice) = 0;
    virtual void remove(const std::string& catalogName) = 0;
    virtual void addManual(const std::string& name, const std::string& address, bool ps5) = 0;
    virtual void findRemote() = 0;
    virtual auto findsRemote() const -> bool = 0;
};

}
