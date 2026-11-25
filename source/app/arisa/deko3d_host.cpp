#include "app/arisa/deko3d_host.hpp"

#include "app/arisa/deko3d_presenter.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

Deko3dHost::Deko3dHost(provider::horizon::Deko3dGpu& gpu, const provider::SurfaceProvider& surface,
    OverlayHandoff& handoff)
    : gpu(gpu)
    , surface(surface)
    , overlayHandoff(handoff)
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
    privateQueue.waitIdle();
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

void Deko3dHost::bindDescriptors(dk::CmdBuf commands) const
{
    commands.bindImageDescriptorSet(imageDescriptors.getGpuAddr(), imageSlots);
    commands.bindSamplerDescriptorSet(samplerDescriptors.getGpuAddr(), 1);
    commands.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
}

void Deko3dHost::waitIdle()
{
    privateQueue.waitIdle();
    Deko3dPresenter* presenter = nullptr;
    {
        const std::scoped_lock lock(presenting);
        presenter = active;
    }
    if (presenter)
        presenter->waitIdle();
}

auto Deko3dHost::present(platform::deko3d::FramePass& pass) -> std::unique_ptr<platform::deko3d::Presentation>
{
    return std::make_unique<Deko3dPresenter>(*this, pass);
}

void Deko3dHost::attach(Deko3dPresenter* presenter)
{
    const std::scoped_lock lock(presenting);
    active = presenter;
}

void Deko3dHost::detach(Deko3dPresenter* presenter)
{
    const std::scoped_lock lock(presenting);
    if (active == presenter)
        active = nullptr;
}

auto Deko3dHost::captureStream(StreamCapture::Delivery& delivery) -> bool
{
    const std::scoped_lock lock(presenting);
    if (!active || !active->takenOver())
        return false;
    active->capture(std::move(delivery));
    return true;
}

}
