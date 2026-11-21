#pragma once

#include <cstdint>
#include <optional>

#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/button_capture.hpp>

#include "play/button_capture.hpp"

namespace akira::app::arisa {

class PadCapture final : public play::ButtonCapture {
public:
    explicit PadCapture(::arisa::runtime::Application& application);

    void begin() override;
    auto held() -> std::uint64_t override;
    void end() override;

private:
    static auto bitsOf(::arisa::provider::Buttons buttons) -> std::uint64_t;

    ::arisa::runtime::Application& application;
    std::optional<::arisa::runtime::ButtonCapture> capture;
};

}
