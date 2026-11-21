#pragma once

#include <cstdint>
#include <optional>

#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/button_capture.hpp>

#include "app/arisa/stream_stage.hpp"
#include "play/button_capture.hpp"

namespace akira::app::arisa {

class PadCapture final : public play::ButtonCapture {
public:
    explicit PadCapture(StreamStage& stage);

    void begin() override;
    auto held() -> std::uint64_t override;
    void end() override;

private:
    static auto bitsOf(::arisa::provider::Buttons buttons) -> std::uint64_t;

    StreamStage& stage;
    std::optional<::arisa::runtime::ButtonCapture> capture;
};

}
