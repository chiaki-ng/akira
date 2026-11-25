#include "app/views/borealis_deko3d_host.hpp"

#ifdef BOREALIS_USE_DEKO3D

#include <borealis.hpp>

namespace akira::app::views {

auto BorealisDeko3dHost::context() -> brls::SwitchDeko3dVideoContext&
{
    return *static_cast<brls::SwitchDeko3dVideoContext*>(brls::Application::getPlatform()->getVideoContext());
}

auto BorealisDeko3dHost::device() const -> dk::Device
{
    return context().getDeko3dDevice();
}

auto BorealisDeko3dHost::queue() const -> dk::Queue
{
    return context().getQueue();
}

auto BorealisDeko3dHost::images() -> CMemPool&
{
    return *context().getImagesPool();
}

auto BorealisDeko3dHost::framesInFlight() const -> std::uint32_t
{
    return brls::FRAMEBUFFERS_COUNT;
}

auto BorealisDeko3dHost::displaySize() const -> DisplaySize
{
    return DisplaySize{ .width = static_cast<int>(brls::Application::windowWidth),
        .height = static_cast<int>(brls::Application::windowHeight) };
}

auto BorealisDeko3dHost::allocateImageSlot() -> int
{
    return context().allocateImageIndex();
}

void BorealisDeko3dHost::freeImageSlot(int slot)
{
    context().freeImageIndex(slot);
}

auto BorealisDeko3dHost::writeImageSlot(dk::CmdBuf commands, int slot, const dk::ImageDescriptor& descriptor) -> bool
{
    return context().updateImageDescriptor(commands, slot, descriptor);
}

void BorealisDeko3dHost::invalidateImageSlots(dk::CmdBuf commands)
{
    context().invalidateImageDescriptors(commands);
}

void BorealisDeko3dHost::waitIdle()
{
    context().getQueue().waitIdle();
}

auto BorealisDeko3dHost::present(platform::deko3d::FramePass& pass) -> std::unique_ptr<platform::deko3d::Presentation>
{
    return std::make_unique<Frames>(pass);
}

BorealisDeko3dHost::Recorder::Recorder(dk::Queue queue, dk::Image& framebuffer)
    : queue(queue)
    , framebuffer(framebuffer)
{
}

void BorealisDeko3dHost::Recorder::run(DkCmdList list)
{
    queue.submitCommands(list);
}

void BorealisDeko3dHost::Recorder::settle()
{
    queue.waitIdle();
}

void BorealisDeko3dHost::Recorder::flush()
{
    queue.flush();
}

auto BorealisDeko3dHost::Recorder::target() -> dk::Image&
{
    return framebuffer;
}

BorealisDeko3dHost::Frames::Frames(platform::deko3d::FramePass& pass)
    : pass(pass)
{
}

void BorealisDeko3dHost::Frames::frameReady()
{
    VideoContext* video = brls::Application::getPlatform()->getVideoContext();
    video->beginFrame();
    if (dk::Image* framebuffer = context().getFramebuffer())
    {
        Recorder recorder(context().getQueue(), *framebuffer);
        pass.record(recorder);
    }
    video->endFrame();
}

}

#endif
