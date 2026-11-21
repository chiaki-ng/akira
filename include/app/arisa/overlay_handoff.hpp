#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <variant>

#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/surface/surface_size.hpp>

namespace akira::app::arisa {

class OverlayHandoff {
public:
    struct Offer {
        ::arisa::provider::FrameSource frames;
        ::arisa::provider::SurfaceSize extent;
    };

    struct Refusal {
        std::string reason;
    };

    using Event = std::variant<Offer, Refusal>;

    void offer(::arisa::provider::FrameSource frames, ::arisa::provider::SurfaceSize extent);
    void refuse(std::string reason);
    void withdraw();
    [[nodiscard]] auto take() -> std::optional<Event>;

private:
    std::mutex mutex;
    std::optional<Event> pending;
};

}
