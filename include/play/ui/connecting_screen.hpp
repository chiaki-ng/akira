#pragma once

#include <string>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "play/connect_flow.hpp"
#include "play/ui/stream_commands.hpp"

namespace akira::play::ui {

class ConnectingScreen final : public ::arisa::composition::Component {
public:
    static constexpr int segments = 4;
    static constexpr float titleSize = 30.0F;
    static constexpr float titleTracking = -0.7F;
    static constexpr float cardWidth = 420.0F;
    static constexpr float logoHeight = 70.0F;
    static constexpr float logoAspect = 2044.0F / 1215.0F;
    static constexpr float mascotSize = logoHeight * 1.35F;
    static constexpr float mascotRadius = mascotSize * 0.22F;
    static constexpr float lockupGap = 22.0F;
    static constexpr const char* logoImage = "img/chiaking.png";
    static constexpr const char* mascotImage = "img/icon.jpg";

    struct Progress {
        std::string console;
        std::string status;
        int step = 1;
        int steps = ConnectFlow::stageCount;

        friend bool operator==(const Progress&, const Progress&) = default;
    };

    ConnectingScreen(Progress progress, StreamCommands& commands, const app::Strings& strings);

    bool operator==(const ConnectingScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto progressOf(const ConnectFlow::Ring& ring, std::string console, const app::Strings& strings) -> Progress;
    static auto activeSegment(int step, int steps) -> int;

private:
    auto bar() const -> ::arisa::composition::Element;
    static auto lockup() -> ::arisa::composition::Element;

    Progress progress;
    StreamCommands* commands;
    const app::Strings* strings;
};

}
