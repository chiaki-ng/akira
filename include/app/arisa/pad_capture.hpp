#pragma once

#include <cstdint>
#include <optional>

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/input/stick.hpp>
#include <arisa/runtime/button_capture.hpp>

#include "app/arisa/stream_stage.hpp"
#include "play/button_capture.hpp"

namespace akira::app::arisa {

class PadCapture final : public play::ButtonCapture {
public:
    static constexpr float stickThreshold = 0.5F;

    explicit PadCapture(StreamStage& stage);

    void begin() override;
    auto held() -> std::uint64_t override;
    void end() override;

    static auto bitsOf(const ::arisa::provider::PadState& pad) -> std::uint64_t;

private:
    enum class Side { Left, Right };

    static auto bitsOf(::arisa::provider::Buttons buttons) -> std::uint64_t;
    static auto bitsOf(::arisa::provider::Stick stick, Side side) -> std::uint64_t;

    StreamStage& stage;
    std::optional<::arisa::runtime::ButtonCapture> capture;
};

}
