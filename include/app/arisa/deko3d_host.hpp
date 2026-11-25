#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>

#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <nanovg/framework/CMemPool.h>

#include "app/arisa/overlay_handoff.hpp"
#include "app/arisa/stream_capture.hpp"
#include "platform/video/deko3d/gpu_host.hpp"

namespace akira::app::arisa {

class Deko3dPresenter;

class Deko3dHost final : public platform::deko3d::GpuHost {
public:
    static constexpr std::uint32_t imageSlots = 32;
    static constexpr std::uint32_t framesBuffered = 3;

    Deko3dHost(::arisa::provider::horizon::Deko3dGpu& gpu, const ::arisa::provider::SurfaceProvider& surface,
        OverlayHandoff& handoff);
    ~Deko3dHost() override;

    Deko3dHost(const Deko3dHost&) = delete;
    Deko3dHost& operator=(const Deko3dHost&) = delete;

    auto device() const -> dk::Device override;
    auto queue() const -> dk::Queue override;
    auto images() -> CMemPool& override;
    auto framesInFlight() const -> std::uint32_t override;
    auto displaySize() const -> DisplaySize override;
    auto allocateImageSlot() -> int override;
    void freeImageSlot(int slot) override;
    auto writeImageSlot(dk::CmdBuf commands, int slot, const dk::ImageDescriptor& descriptor) -> bool override;
    void invalidateImageSlots(dk::CmdBuf commands) override;
    void waitIdle() override;
    auto present(platform::deko3d::FramePass& pass) -> std::unique_ptr<platform::deko3d::Presentation> override;

    auto arisa() -> ::arisa::provider::horizon::Deko3dGpu& { return gpu; }
    auto handoff() -> OverlayHandoff& { return overlayHandoff; }
    void bindDescriptors(dk::CmdBuf commands) const;
    void attach(Deko3dPresenter* presenter);
    void detach(Deko3dPresenter* presenter);
    [[nodiscard]] auto captureStream(StreamCapture::Delivery& delivery) -> bool;

private:
    void writeSampler();

    ::arisa::provider::horizon::Deko3dGpu& gpu;
    const ::arisa::provider::SurfaceProvider& surface;
    OverlayHandoff& overlayHandoff;
    dk::UniqueQueue privateQueue;
    CMemPool imagePool;
    CMemPool dataPool;
    CMemPool::Handle imageDescriptors;
    CMemPool::Handle samplerDescriptors;
    std::array<bool, imageSlots> usedSlots{};
    std::mutex presenting;
    Deko3dPresenter* active = nullptr;
};

}
