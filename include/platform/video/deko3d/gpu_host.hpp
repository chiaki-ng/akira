#pragma once

#include <cstdint>
#include <memory>

#if defined(__SWITCH2__)
#include <drako3d.hpp>
#else
#include <deko3d.hpp>
#endif

class CMemPool;

namespace akira::platform::deko3d {

class FrameRecorder {
public:
    virtual ~FrameRecorder() = default;

    virtual void run(DkCmdList list) = 0;
    virtual void settle() = 0;
    virtual void flush() = 0;
    virtual auto target() -> dk::Image& = 0;
};

class FramePass {
public:
    virtual ~FramePass() = default;

    virtual void record(FrameRecorder& frame) = 0;
};

class Presentation {
public:
    virtual ~Presentation() = default;

    virtual void frameReady() = 0;
    virtual void paused(bool paused) { static_cast<void>(paused); }
};

class GpuHost {
public:
    struct DisplaySize {
        int width;
        int height;
    };

    virtual ~GpuHost() = default;

    virtual auto device() const -> dk::Device = 0;
    virtual auto queue() const -> dk::Queue = 0;
    virtual auto images() -> CMemPool& = 0;
    virtual auto framesInFlight() const -> std::uint32_t = 0;
    virtual auto displaySize() const -> DisplaySize = 0;
    virtual auto allocateImageSlot() -> int = 0;
    virtual void freeImageSlot(int slot) = 0;
    virtual auto writeImageSlot(dk::CmdBuf commands, int slot, const dk::ImageDescriptor& descriptor) -> bool = 0;
    virtual void invalidateImageSlots(dk::CmdBuf commands) = 0;
    virtual void waitIdle() = 0;
    virtual auto present(FramePass& pass) -> std::unique_ptr<Presentation> = 0;
};

}
