#pragma once

#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include "app/strings.hpp"
#include "play/connect_flow.hpp"
#include "play/ui/stream_commands.hpp"

namespace akira::play::ui {

class ConnectingScreen final : public ::arisa::composition::Component {
public:
    enum class Capsule { Done, Active, Waiting };

    static constexpr float titleSize = 30.0F;
    static constexpr float titleTracking = -0.7F;
    static constexpr float cardWidth = 420.0F;
    static constexpr float logoHeight = 70.0F;
    static constexpr float mascotScale = 1.35F;
    static constexpr float capsuleWidth = 26.0F;
    static constexpr float activeCapsuleWidth = 34.0F;
    static constexpr float capsuleHeight = 6.0F;
    static constexpr float capsuleSpacing = 9.0F;
    static constexpr float capsuleGlow = 7.0F;
    static constexpr float backdropDim = 0.12F;
    static constexpr float detailWidth = 520.0F;
    static constexpr std::size_t logRows = 9;
    static constexpr float logWidth = 640.0F;
    static constexpr float logHeight = 168.0F;

    struct Progress {
        std::string console;
        std::string status;
        int step = 1;
        int steps = ConnectFlow::stageCount;
        std::string activity;
        std::string details;
        bool stages = true;
        std::vector<std::string> log;

        friend bool operator==(const Progress&, const Progress&) = default;
    };

    ConnectingScreen(Progress progress, StreamCommands& commands, const app::Strings& strings);

    bool operator==(const ConnectingScreen& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto progressOf(const ConnectFlow::Ring& ring, std::string console, const app::Strings& strings) -> Progress;
    static auto capsule(int index, int step) -> Capsule;

private:
    auto bar() const -> ::arisa::composition::Element;
    auto operation() const -> ::arisa::composition::Element;
    auto stagesShown() const -> ::arisa::composition::Element;
    auto logShown() const -> ::arisa::composition::Element;
    static auto pill(Capsule state) -> ::arisa::composition::Element;

    Progress progress;
    StreamCommands* commands;
    const app::Strings* strings;
};

}
