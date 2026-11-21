#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>

#include "app/arisa/stream_capture.hpp"
#include "platform/video/deko3d/gpu_host.hpp"
#include "play/present_loop.hpp"

namespace akira::app::arisa {

class Deko3dHost;

class Deko3dPresenter final : public platform::deko3d::Presentation, private play::PresentLoop::Target {
public:
    Deko3dPresenter(Deko3dHost& host, platform::deko3d::FramePass& pass);
    ~Deko3dPresenter() override;

    Deko3dPresenter(const Deko3dPresenter&) = delete;
    Deko3dPresenter& operator=(const Deko3dPresenter&) = delete;

    void frameReady() override;
    void paused(bool paused) override;

    auto takenOver() const -> bool { return seized.load(std::memory_order_acquire); }
    void capture(StreamCapture::Delivery delivery);
    void waitIdle();

private:
    using Gpu = ::arisa::provider::horizon::Deko3dGpu;

    static constexpr std::uint32_t slotCount = 3;
    static constexpr std::uint32_t chunkSize = 256 * 1024;
    static constexpr std::uint32_t pixelBytes = 4;

    class CommandSlot {
    public:
        explicit CommandSlot(dk::Device device);

        CommandSlot(const CommandSlot&) = delete;
        CommandSlot& operator=(const CommandSlot&) = delete;

        auto idle() -> bool;
        auto begin() -> dk::CmdBuf;
        void submit(dk::Queue queue);

    private:
        static void grow(void* user, DkCmdBuf buffer, std::size_t minimum);
        void add(std::size_t minimum);

        dk::Device device;
        dk::UniqueCmdBuf commands;
        std::vector<dk::UniqueMemBlock> memory;
        dk::Fence done{};
        bool pending = false;
    };

    class Recorder final : public platform::deko3d::FrameRecorder {
    public:
        Recorder(const Deko3dHost& host, dk::CmdBuf commands, const Gpu::FrameTarget& frame);

        void run(DkCmdList list) override;
        void settle() override;
        void flush() override;
        auto target() -> dk::Image& override;

    private:
        dk::CmdBuf commands;
        const Gpu::FrameTarget& frame;
    };

    class Readback {
    public:
        Readback(dk::Device device, std::uint32_t width, std::uint32_t height, StreamCapture::Delivery delivery);

        void record(dk::CmdBuf commands, const Gpu::FrameTarget& frame);
        void submit(dk::Queue queue);
        auto ready() -> bool;
        void deliver();
        void refuse(std::string reason);

    private:
        std::uint32_t width;
        std::uint32_t height;
        dk::UniqueMemBlock memory;
        dk::Fence done{};
        bool submitted = false;
        StreamCapture::Delivery delivery;
    };

    auto present(play::PresentTiming::Kind kind, play::PresentTiming& timing)
        -> std::expected<play::PresentLoop::Presented, play::PresentTiming::Miss> override;
    void swapInterval(std::uint32_t interval) override;
    auto awaitScene(std::chrono::milliseconds limit) -> play::PresentLoop::Scene override;
    [[nodiscard]] auto seize() -> bool;
    void follow(const Gpu::FrameTarget& frame);
    void collectReadback();
    void startReadback(dk::CmdBuf commands, const Gpu::FrameTarget& frame);

    Deko3dHost& host;
    platform::deko3d::FramePass& pass;
    std::array<std::unique_ptr<CommandSlot>, slotCount> slots;
    std::uint32_t next = 0;
    std::optional<Gpu::Takeover> takeover;
    std::optional<Gpu::Overlay> overlay;
    std::optional<::arisa::provider::SwapInterval> interval;
    std::atomic<std::shared_ptr<const ::arisa::provider::FrameSource>> scenes;
    std::uint64_t seenScene = 0;
    std::uint64_t lentGeneration = 0;
    bool refused = false;
    std::uint64_t overlayFailures = 0;
    std::atomic<bool> seized{ false };
    std::mutex captureMutex;
    std::optional<StreamCapture::Delivery> wanted;
    std::optional<Readback> reading;
    play::PresentLoop loop;
};

}
