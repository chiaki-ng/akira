#include "app/arisa/deko3d_presenter.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>
#include <utility>

#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include "app/arisa/deko3d_host.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;
using Clock = play::PresentLoop::Clock;

Deko3dPresenter::CommandSlot::CommandSlot(dk::Device device)
    : device(device)
    , commands(dk::CmdBufMaker{device}.setUserData(this).setCbAddMem(&CommandSlot::grow).create())
{
    add(chunkSize);
}

void Deko3dPresenter::CommandSlot::grow(void* user, DkCmdBuf, std::size_t minimum)
{
    static_cast<CommandSlot*>(user)->add(minimum);
}

void Deko3dPresenter::CommandSlot::add(std::size_t minimum)
{
    const std::size_t wanted = std::max<std::size_t>(chunkSize, minimum);
    const auto size = static_cast<std::uint32_t>((wanted + DK_MEMBLOCK_ALIGNMENT - 1) & ~std::size_t{ DK_MEMBLOCK_ALIGNMENT - 1 });
    dk::UniqueMemBlock block = dk::MemBlockMaker{device, size}
                                   .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
                                   .create();
    commands.addMemory(block, 0, size);
    memory.push_back(std::move(block));
}

auto Deko3dPresenter::CommandSlot::idle() -> bool
{
    if (!pending)
        return true;
    if (done.wait(0) != DkResult_Success)
        return false;
    pending = false;
    return true;
}

auto Deko3dPresenter::CommandSlot::begin() -> dk::CmdBuf
{
    commands.clear();
    if (memory.size() > 1)
        memory.erase(memory.begin(), memory.end() - 1);
    return commands;
}

void Deko3dPresenter::CommandSlot::submit(dk::Queue queue)
{
    queue.submitCommands(commands.finishList());
    queue.signalFence(done, false);
    pending = true;
}

Deko3dPresenter::Recorder::Recorder(const Deko3dHost& host, dk::CmdBuf commands, const Gpu::FrameTarget& frame)
    : commands(commands)
    , frame(frame)
{
    commands.bindRenderTargets(&frame.view);
    commands.setViewports(0, { { 0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height), 0.0f,
        1.0f } });
    commands.setScissors(0, { { 0, 0, frame.width, frame.height } });
    commands.clearColor(0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    host.bindDescriptors(commands);
}

void Deko3dPresenter::Recorder::run(DkCmdList list)
{
    commands.callList(list);
}

void Deko3dPresenter::Recorder::settle()
{
    commands.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
}

void Deko3dPresenter::Recorder::flush()
{
}

auto Deko3dPresenter::Recorder::target() -> dk::Image&
{
    return frame.image;
}

Deko3dPresenter::Readback::Readback(dk::Device device, std::uint32_t width, std::uint32_t height,
    StreamCapture::Delivery delivery)
    : width(width)
    , height(height)
    , memory(dk::MemBlockMaker{device,
          static_cast<std::uint32_t>((std::size_t{ width } * height * pixelBytes + DK_MEMBLOCK_ALIGNMENT - 1)
              & ~std::size_t{ DK_MEMBLOCK_ALIGNMENT - 1 })}
                 .setFlags(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached)
                 .create())
    , delivery(std::move(delivery))
{
}

void Deko3dPresenter::Readback::record(dk::CmdBuf commands, const Gpu::FrameTarget& frame)
{
    commands.barrier(DkBarrier_Fragments, 0);
    commands.copyImageToBuffer(dk::ImageView{ frame.image },
        DkImageRect{ .x = 0, .y = 0, .z = 0, .width = width, .height = height, .depth = 1 },
        DkCopyBuf{ .addr = memory.getGpuAddr(), .rowLength = 0, .imageHeight = 0 });
}

void Deko3dPresenter::Readback::submit(dk::Queue queue)
{
    if (submitted)
        return;
    queue.signalFence(done, false);
    submitted = true;
}

auto Deko3dPresenter::Readback::ready() -> bool
{
    return submitted && done.wait(0) == DkResult_Success;
}

void Deko3dPresenter::Readback::deliver()
{
    const std::size_t bytes = std::size_t{ width } * height * pixelBytes;
    const auto* mapped = static_cast<const std::byte*>(memory.getCpuAddr());
    std::optional<Screenshot> shot = Screenshot::create(width, height, std::vector<std::byte>(mapped, mapped + bytes));
    if (!shot)
    {
        refuse("the stream frame could not be read back");
        return;
    }
    if (delivery)
        std::exchange(delivery, nullptr)(std::move(*shot));
}

void Deko3dPresenter::Readback::refuse(std::string reason)
{
    if (delivery)
        std::exchange(delivery, nullptr)(std::unexpected(std::move(reason)));
}

Deko3dPresenter::Deko3dPresenter(Deko3dHost& host, platform::deko3d::FramePass& pass)
    : host(host)
    , pass(pass)
    , loop(*this, play::PresentLoop::Pacing{})
{
    for (std::unique_ptr<CommandSlot>& slot : slots)
        slot = std::make_unique<CommandSlot>(host.device());
    host.attach(this);
}

Deko3dPresenter::~Deko3dPresenter()
{
    host.detach(this);
    host.handoff().withdraw();
    loop.stop();
    {
        const std::scoped_lock lock(captureMutex);
        if (wanted)
            std::exchange(wanted, std::nullopt).value()(std::unexpected(std::string("the stream ended")));
        if (reading)
            reading->refuse("the stream ended before the frame was read back");
    }
    if (takeover)
        if (std::optional<dk::Queue> queue = takeover->queue())
            queue->waitIdle();
    log::info("[PRESENT] handing the GPU back to arisa after {} presents", takeover ? takeover->presents() : 0);
}

void Deko3dPresenter::frameReady()
{
    loop.frameReady();
}

void Deko3dPresenter::paused(bool paused)
{
    loop.paused(paused);
}

void Deko3dPresenter::capture(StreamCapture::Delivery delivery)
{
    const std::scoped_lock lock(captureMutex);
    if (wanted || reading)
    {
        delivery(std::unexpected(std::string("a screenshot is already being taken")));
        return;
    }
    wanted.emplace(std::move(delivery));
}

void Deko3dPresenter::waitIdle()
{
    loop.serialized([this] {
        if (takeover)
            if (std::optional<dk::Queue> queue = takeover->queue())
                queue->waitIdle();
    });
}

auto Deko3dPresenter::seize() -> bool
{
    std::expected<Gpu::Takeover, provider::ProviderError> taken = host.arisa().take_over();
    if (!taken)
    {
        const std::string reason = "arisa refused the GPU takeover: " + taken.error().explanation();
        log::error("[PRESENT] {}", reason);
        host.handoff().refuse(reason);
        return false;
    }
    takeover.emplace(std::move(*taken));
    const std::optional<Gpu::FrameTarget> first = takeover->target(0);
    std::expected<Gpu::Overlay, provider::ProviderError> opened = takeover->overlay();
    if (!first || !opened)
    {
        const std::string reason = !opened ? "arisa refused the stream overlay: " + opened.error().explanation()
                                           : std::string("arisa lent no swapchain image");
        log::error("[PRESENT] {}", reason);
        host.handoff().refuse(reason);
        overlay.reset();
        takeover.reset();
        return false;
    }
    overlay.emplace(std::move(*opened));
    if (interval)
        takeover->set_swap_interval(*interval);
    const provider::SurfaceSize extent{ .width = provider::SurfaceWidth{ first->width },
        .height = provider::SurfaceHeight{ first->height } };
    host.handoff().offer(overlay->frames(), extent);
    seized.store(true, std::memory_order_release);
    log::info("[PRESENT] took the GPU over: {} lent images at {}x{}, presenting from the stream thread",
        takeover->image_count(), first->width, first->height);
    return true;
}

void Deko3dPresenter::swapInterval(std::uint32_t value)
{
    interval = provider::SwapInterval::create(value);
    if (takeover && interval)
        takeover->set_swap_interval(*interval);
}

void Deko3dPresenter::collectReadback()
{
    const std::scoped_lock lock(captureMutex);
    if (!reading || !reading->ready())
        return;
    reading->deliver();
    reading.reset();
}

void Deko3dPresenter::startReadback(dk::CmdBuf commands, const Gpu::FrameTarget& frame)
{
    const std::scoped_lock lock(captureMutex);
    if (!wanted || reading)
        return;
    reading.emplace(host.device(), frame.width, frame.height, std::exchange(wanted, std::nullopt).value());
    reading->record(commands, frame);
}

auto Deko3dPresenter::present(play::PresentTiming::Kind, play::PresentTiming&)
    -> std::expected<play::PresentLoop::Presented, play::PresentTiming::Miss>
{
    if (!takeover)
    {
        if (refused)
            return std::unexpected(play::PresentTiming::Miss::unavailable);
        if (!seize())
        {
            refused = true;
            return std::unexpected(play::PresentTiming::Miss::unavailable);
        }
    }
    collectReadback();
    CommandSlot& slot = *slots[next];
    if (!slot.idle())
        return std::unexpected(play::PresentTiming::Miss::busy);
    const std::expected<std::uint32_t, provider::ProviderError> index = takeover->acquire();
    if (!index)
    {
        log::warning("[PRESENT] no lent image: {}", index.error().explanation());
        return std::unexpected(play::PresentTiming::Miss::unavailable);
    }
    const std::optional<Gpu::FrameTarget> frame = takeover->target(*index);
    const std::optional<dk::Queue> queue = takeover->queue();
    if (!frame || !queue)
    {
        static_cast<void>(takeover->present(*index));
        return std::unexpected(play::PresentTiming::Miss::unavailable);
    }
    dk::CmdBuf commands = slot.begin();
    {
        Recorder recorder(host, commands, *frame);
        pass.record(recorder);
    }
    if (overlay)
    {
        if (const std::expected<bool, provider::ProviderError> recorded = overlay->record(commands, *frame);
            !recorded && overlayFailures++ % 120 == 0)
            log::warning("[PRESENT] the overlay was not recorded: {}", recorded.error().explanation());
    }
    startReadback(commands, *frame);
    slot.submit(*queue);
    {
        const std::scoped_lock lock(captureMutex);
        if (reading)
            reading->submit(*queue);
    }
    const Clock::time_point submitted = Clock::now();
    if (const std::expected<void, provider::ProviderError> shown = takeover->present(*index); !shown)
        log::warning("[PRESENT] present refused: {}", shown.error().explanation());
    const Clock::time_point presented = Clock::now();
    next = (next + 1) % slotCount;
    return play::PresentLoop::Presented{ .submitted = submitted, .presented = presented };
}

}
