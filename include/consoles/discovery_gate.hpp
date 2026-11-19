#pragma once

namespace akira::consoles {

class DiscoveryGate {
public:
    enum class Discovered { NoHost, Suspended, NoList, Update };

    void appeared() { m_shown = true; }
    void disappeared() { m_shown = false; }
    void connectionStarted() { m_connecting = true; }
    void connectionEnded() { m_connecting = false; }

    bool shown() const { return m_shown; }
    bool open() const { return m_shown && !m_connecting; }

    Discovered discovered(bool hostPresent, bool listPresent) const;
    bool hostsChanged(bool listPresent) const { return open() && listPresent; }

private:
    bool m_shown = false;
    bool m_connecting = false;
};

}
