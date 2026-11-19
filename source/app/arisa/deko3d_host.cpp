#include "app/arisa/deko3d_host.hpp"

#include <switch.h>

#include <chrono>

#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

Deko3dHost::Deko3dHost(provider::horizon::Deko3dGpu& gpu, const provider::SurfaceProvider& surface)
    : gpu(gpu)
    , surface(surface)
    , privateQueue(dk::QueueMaker{gpu.device()}.setFlags(DkQueueFlags_Graphics).create())
    , imagePool(gpu.device(), DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16 * 1024 * 1024)
    , dataPool(gpu.device(), DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 256 * 1024)
    , imageDescriptors(dataPool.allocate(imageSlots * sizeof(DkImageDescriptor), DK_IMAGE_DESCRIPTOR_ALIGNMENT))
    , samplerDescriptors(dataPool.allocate(sizeof(DkSamplerDescriptor), DK_SAMPLER_DESCRIPTOR_ALIGNMENT))
{
    writeSampler();
}

Deko3dHost::~Deko3dHost()
{
    waitIdle();
    imageDescriptors.destroy();
    samplerDescriptors.destroy();
}

void Deko3dHost::writeSampler()
{
    dk::SamplerDescriptor descriptor;
    descriptor.initialize(dk::Sampler{}
        .setFilter(DkFilter_Linear, DkFilter_Linear, DkMipFilter_None)
        .setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge));
    CMemPool::Handle memory = dataPool.allocate(DK_MEMBLOCK_ALIGNMENT);
    dk::UniqueCmdBuf commands = dk::CmdBufMaker{gpu.device()}.create();
    commands.addMemory(memory.getMemBlock(), memory.getOffset(), memory.getSize());
    commands.pushData(samplerDescriptors.getGpuAddr(), &descriptor, sizeof(descriptor));
    commands.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
    privateQueue.submitCommands(commands.finishList());
    privateQueue.waitIdle();
    memory.destroy();
}

auto Deko3dHost::device() const -> dk::Device
{
    return gpu.device();
}

auto Deko3dHost::queue() const -> dk::Queue
{
    return privateQueue;
}

auto Deko3dHost::images() -> CMemPool&
{
    return imagePool;
}

auto Deko3dHost::framesInFlight() const -> std::uint32_t
{
    return framesBuffered;
}

auto Deko3dHost::displaySize() const -> DisplaySize
{
    const provider::SurfaceSize size = surface.size();
    return DisplaySize{ .width = static_cast<int>(size.width.value()), .height = static_cast<int>(size.height.value()) };
}

auto Deko3dHost::allocateImageSlot() -> int
{
    for (std::uint32_t slot = 0; slot < imageSlots; ++slot)
    {
        if (!usedSlots[slot])
        {
            usedSlots[slot] = true;
            return static_cast<int>(slot);
        }
    }
    log::error("[ARISA] the video pass ran out of image slots");
    return 0;
}

void Deko3dHost::freeImageSlot(int slot)
{
    if (slot >= 0 && static_cast<std::uint32_t>(slot) < imageSlots)
        usedSlots[static_cast<std::uint32_t>(slot)] = false;
}

auto Deko3dHost::writeImageSlot(dk::CmdBuf commands, int slot, const dk::ImageDescriptor& descriptor) -> bool
{
    if (slot < 0 || static_cast<std::uint32_t>(slot) >= imageSlots)
        return false;
    commands.pushData(imageDescriptors.getGpuAddr() + static_cast<DkGpuAddr>(slot) * sizeof(DkImageDescriptor),
        &descriptor, sizeof(DkImageDescriptor));
    return true;
}

void Deko3dHost::invalidateImageSlots(dk::CmdBuf commands)
{
    commands.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
}

void Deko3dHost::waitIdle()
{
    using namespace std::chrono_literals;
    privateQueue.waitIdle();
    if (!recorded.load(std::memory_order_acquire))
        return;
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    const provider::GpuSerial serial{lastSerial.load(std::memory_order_acquire)};
    while (!gpu.completed(serial) && std::chrono::steady_clock::now() < deadline)
        svcSleepThread(1'000'000);
}

auto Deko3dHost::present(platform::deko3d::FramePass& pass) -> std::unique_ptr<platform::deko3d::Presentation>
{
    return std::make_unique<Frames>(*this, pass);
}

void Deko3dHost::record(platform::deko3d::FramePass& pass, dk::CmdBuf& commands, const FrameTarget& frame,
    const provider::FrameInfo& info)
{
    Recorder recorder(*this, commands, frame);
    pass.record(recorder);
    lastSerial.store(info.serial.value(), std::memory_order_release);
    recorded.store(true, std::memory_order_release);
}

Deko3dHost::Recorder::Recorder(Deko3dHost& host, dk::CmdBuf& commands, const FrameTarget& frame)
    : commands(commands)
    , frame(frame)
{
    commands.bindRenderTargets(&frame.view);
    commands.setViewports(0, { { 0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height), 0.0f,
        1.0f } });
    commands.setScissors(0, { { 0, 0, frame.width, frame.height } });
    commands.bindImageDescriptorSet(host.imageDescriptors.getGpuAddr(), imageSlots);
    commands.bindSamplerDescriptorSet(host.samplerDescriptors.getGpuAddr(), 1);
    commands.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
}

void Deko3dHost::Recorder::run(DkCmdList list)
{
    commands.callList(list);
}

void Deko3dHost::Recorder::settle()
{
    commands.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
}

void Deko3dHost::Recorder::flush()
{
}

auto Deko3dHost::Recorder::target() -> dk::Image&
{
    return frame.image;
}

Deko3dHost::Frames::Frames(Deko3dHost& host, platform::deko3d::FramePass& pass)
    : hook(host.gpu.add_frame_hook(provider::FrameStage::before_ui,
          [&host, &pass](dk::CmdBuf& commands, const FrameTarget& frame, const provider::FrameInfo& info) {
              host.record(pass, commands, frame, info);
          }))
{
}

void Deko3dHost::Frames::frameReady()
{
    hook.request_frame();
}

}
