#pragma once

#include "play/diagnostics_appearance.hpp"
#include "play/diagnostics_report.hpp"

namespace akira::play::ui {

class StreamCommands {
public:
    virtual ~StreamCommands() = default;

    virtual void cancel() = 0;
    virtual void reconnect() = 0;
    virtual void resume() = 0;
    virtual void leave(bool rest) = 0;
    virtual void resetMotion() = 0;
    virtual void showDiagnostics(DiagnosticsAppearance appearance) = 0;
    virtual auto diagnostics() -> DiagnosticsReport = 0;
};

}
