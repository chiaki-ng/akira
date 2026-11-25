#pragma once

#ifdef BOREALIS_USE_DEKO3D

#include <borealis/platforms/switch/switch_video.hpp>

#include "platform/video/deko3d/gpu_host.hpp"

namespace akira::app::views {

class BorealisDeko3dHost : public platform::deko3d::GpuHost {
public:
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

private:
    class Recorder final : public platform::deko3d::FrameRecorder {
    public:
        Recorder(dk::Queue queue, dk::Image& framebuffer);

        void run(DkCmdList list) override;
        void settle() override;
        void flush() override;
        auto target() -> dk::Image& override;

    private:
        dk::Queue queue;
        dk::Image& framebuffer;
    };

    class Frames final : public platform::deko3d::Presentation {
    public:
        explicit Frames(platform::deko3d::FramePass& pass);

        void frameReady() override;

    private:
        platform::deko3d::FramePass& pass;
    };

    static auto context() -> brls::SwitchDeko3dVideoContext&;
};

}

#endif
