#pragma once

#include <borealis.hpp>

#include <string>

namespace akira::consoles {
class Host;
}

namespace akira::consoles::views {

class ConsoleCard : public brls::Box {
public:
    explicit ConsoleCard(Host* host);

    void playEntrance(int delayMs);
    void updateState();

    Host* getHost() { return host; }
    std::string getHostName() const { return hostName; }

    void doPrimaryAction();
    void openContextMenu();

private:
    void doConnect();
    void doWake();
    void doRegister();
    void doLink();
    void doConsolePIN();
    void doAddress();
    void doTakionPin();
    void doDelete();

    Host* host;
    std::string hostName;
    brls::Image* consoleImg;
    brls::Label* nameLabel;
    brls::Box* typePill;
    brls::Label* typeLabel;
    brls::Box* statusPill;
    brls::Label* statusLabel;
    brls::Animatable entranceAnim{1.0f};
    brls::Animatable focusAnim{0.0f};
    brls::Animatable dimAnim{1.0f};
    int lastDim = -1;
};

}
