#pragma once

#include <functional>
#include <optional>

#include "play/renderer_kind.hpp"

namespace akira::platform::deko3d {
class GpuHost;
}

namespace akira::platform::vulkan {
class GpuHost;
}

namespace akira::platform::video {

class GpuAccess {
public:
    virtual ~GpuAccess() = default;

    virtual void prefer(play::RendererKind kind) = 0;
    virtual auto deko3d() -> std::optional<std::reference_wrapper<deko3d::GpuHost>> = 0;
    virtual auto vulkan() -> std::optional<std::reference_wrapper<vulkan::GpuHost>> = 0;
};

}
