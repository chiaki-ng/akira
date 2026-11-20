#pragma once

#include <optional>

#include <switch.h>

#include <arisa/runtime/application.hpp>
#include <arisa/runtime/input_block.hpp>

#include "play/button_capture.hpp"

namespace akira::app::arisa {

class PadCapture final : public play::ButtonCapture {
public:
    explicit PadCapture(::arisa::runtime::Application& application);

    void begin() override;
    auto held() -> std::uint64_t override;
    void end() override;

private:
    ::arisa::runtime::Application& application;
    PadState pad{};
    std::optional<::arisa::runtime::InputBlock> blocked;
};

}
