#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <nanovg/framework/CMemPool.h>

#include "platform/video/deko3d/gpu_host.hpp"

namespace akira::app::arisa {

class Deko3dHost final : public platform::deko3d::GpuHost {
public:
    static constexpr std::uint32_t imageSlots = 32;
    static constexpr std::uint32_t framesBuffered = 3;

    Deko3dHost(::arisa::provider::horizon::Deko3dGpu& gpu, const ::arisa::provider::SurfaceProvider& surface);
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

private:
    using FrameTarget = ::arisa::provider::horizon::Deko3dGpu::FrameTarget;

    class Recorder final : public platform::deko3d::FrameRecorder {
    public:
        Recorder(Deko3dHost& host, dk::CmdBuf& commands, const FrameTarget& frame);

        void run(DkCmdList list) override;
        void settle() override;
        void flush() override;
        auto target() -> dk::Image& override;

    private:
        dk::CmdBuf& commands;
        const FrameTarget& frame;
    };

    class Frames final : public platform::deko3d::Presentation {
    public:
        Frames(Deko3dHost& host, platform::deko3d::FramePass& pass);

        void frameReady() override;

    private:
        ::arisa::provider::FrameHook hook;
    };

    void writeSampler();
    void record(platform::deko3d::FramePass& pass, dk::CmdBuf& commands, const FrameTarget& frame,
        const ::arisa::provider::FrameInfo& info);

    ::arisa::provider::horizon::Deko3dGpu& gpu;
    const ::arisa::provider::SurfaceProvider& surface;
    dk::UniqueQueue privateQueue;
    CMemPool imagePool;
    CMemPool dataPool;
    CMemPool::Handle imageDescriptors;
    CMemPool::Handle samplerDescriptors;
    std::array<bool, imageSlots> usedSlots{};
    std::atomic<std::uint64_t> lastSerial{0};
    std::atomic<bool> recorded{false};
};

}
