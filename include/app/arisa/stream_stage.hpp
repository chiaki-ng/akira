#pragma once

#include <optional>
#include <string>

#include <arisa/provider/factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/button_capture.hpp>
#include <arisa/runtime/embedded_root.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/runtime/passed_input.hpp>
#include <arisa/runtime/root.hpp>

#include "app/arisa/notices.hpp"
#include "app/arisa/overlay_handoff.hpp"

namespace akira::app::arisa {

class StreamStage {
public:
    StreamStage(::arisa::runtime::Application& application, ::arisa::provider::ProviderFactory& providers,
        OverlayHandoff& handoff, Notices& notices);
    ~StreamStage();

    StreamStage(const StreamStage&) = delete;
    StreamStage& operator=(const StreamStage&) = delete;

    void follow();
    void close();
    auto embedded() const -> bool;
    [[nodiscard]] auto takeRefusal() -> std::optional<std::string>;

    auto overlays() -> ::arisa::runtime::Overlays&;
    auto blockInput() -> ::arisa::runtime::InputBlock;
    auto passInput(::arisa::provider::Buttons buttons, ::arisa::runtime::PassedInput passed)
        -> ::arisa::runtime::InputPassthrough;
    auto captureButtons() -> ::arisa::runtime::ButtonCapture;

private:
    void embed(OverlayHandoff::Offer offer);
    void resize(::arisa::provider::SurfaceSize extent);
    auto target() -> ::arisa::runtime::Root&;

    ::arisa::runtime::Application& application;
    ::arisa::provider::ProviderFactory& providers;
    OverlayHandoff& handoff;
    Notices& notices;
    std::optional<::arisa::runtime::Root> stage;
    std::optional<::arisa::runtime::EmbeddedRoot> embedding;
    std::optional<std::string> refusal;
};

}
