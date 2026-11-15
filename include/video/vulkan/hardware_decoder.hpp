#ifndef AKIRA_STREAM_HARDWARE_DECODER_HPP
#define AKIRA_STREAM_HARDWARE_DECODER_HPP

#include <switch.h>
#include <vulkan/vulkan.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "video/vulkan/decoder_output.hpp"

namespace akira::stream
{

class HardwareDecoder;

struct HardwarePlane
{
    std::size_t offset;
    std::uint32_t pitch;
};

class HardwareFrame
{
public:
    HardwareFrame(std::shared_ptr<HardwareDecoder> owner, const VideoFrame& frame);
    ~HardwareFrame();
    HardwareFrame(const HardwareFrame&) = delete;
    HardwareFrame& operator=(const HardwareFrame&) = delete;

    std::uint32_t width() const { return m_frame.width; }
    std::uint32_t height() const { return m_frame.height; }
    std::uint32_t slot() const { return m_frame.slot; }
    std::int64_t timestampNs() const { return m_frame.timestamp_ns; }
    std::uint64_t generation() const;
    const void* backing() const;
    std::size_t backingSize() const;
    HardwarePlane luma() const;
    HardwarePlane chroma() const;
    VkBuffer outputBuffer() const;

private:
    std::shared_ptr<HardwareDecoder> m_owner;
    VideoFrame m_frame;
};

class HardwareDecoder : public std::enable_shared_from_this<HardwareDecoder>
{
public:
    using Frame = std::shared_ptr<const HardwareFrame>;

    struct Config
    {
        ViddecCodec codec;
        std::uint32_t width;
        std::uint32_t height;
        std::span<const std::uint8_t> codecConfig;
        std::optional<DecoderOutput::Device> output = std::nullopt;
    };

    static std::expected<std::shared_ptr<HardwareDecoder>, Result> create(const Config& config);

    ~HardwareDecoder();
    HardwareDecoder(const HardwareDecoder&) = delete;
    HardwareDecoder& operator=(const HardwareDecoder&) = delete;

    [[nodiscard]] std::expected<void, Result> submit(std::span<const std::uint8_t> sample, std::int64_t timestampNs,
        std::chrono::nanoseconds timeout);
    std::optional<Frame> nextFrame(std::chrono::nanoseconds timeout);
    [[nodiscard]] std::expected<void, Result> flush();

    std::uint64_t generation() const { return m_generation; }
    std::uint32_t width() const { return m_width; }
    std::uint32_t height() const { return m_height; }
    const void* backing(std::uint32_t slot) const;
    std::size_t backingSize(std::uint32_t slot) const;
    VkBuffer outputBuffer(std::uint32_t slot) const;
    bool zeroCopy() const { return !m_outputs.empty(); }
    const std::optional<DecoderOutput::Failure>& outputFailure() const { return m_output_failure; }
    std::uint32_t takeHeldPeak() { return m_held_peak.exchange(m_held.load(std::memory_order_relaxed)); }
    std::uint64_t reusedWhileHeld() const { return m_reused_while_held.load(std::memory_order_relaxed); }
    void setReporter(std::function<void(const std::string&)> reporter) { m_reporter = std::move(reporter); }

    static constexpr std::uint32_t ImageCount = VIDDEC_MAX_IMAGES;

private:
    friend class HardwareFrame;

    HardwareDecoder(std::uint32_t width, std::uint32_t height);
    void returnFrame(const VideoFrame& frame);
    void releaseReturnedFrames();
    std::expected<void, DecoderOutput::Failure> openWithOutputs(const Config& config, const DecoderOutput::Device& device);
    void closeDecoder();
    static std::uint64_t workSize(std::uint32_t width, std::uint32_t height);

    struct WorkTier
    {
        std::uint32_t width;
        std::uint32_t height;
        std::uint64_t mib;
    };
    static constexpr std::array<WorkTier, 9> WorkTiers{{
        {256, 144, 8}, {384, 216, 9}, {320, 240, 9}, {640, 360, 15}, {854, 480, 20},
        {960, 540, 23}, {1280, 720, 34}, {1920, 1080, 68}, {3840, 2160, 240},
    }};
    static constexpr std::uint64_t WorkAlignment = 0x1000;
    static constexpr std::uint64_t WorkSizeAlignment = 0x10000;
    static constexpr std::uint32_t LargePolicyWidth = 1920;
    static constexpr std::uint32_t LargePolicyHeight = 1080;
    static constexpr std::uint32_t ColorPolicyLarge = 3;
    static constexpr std::uint32_t ColorPolicySmall = 1;
    static constexpr std::uint32_t OutputMode = 1;
    std::string queueState() const;
    static bool timedOut(Result rc);

    ::VideoDecoder m_decoder{};
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint64_t m_generation;
    std::mutex m_returned_mutex;
    std::vector<VideoFrame> m_returned;
    std::vector<std::unique_ptr<DecoderOutput>> m_outputs;
    std::optional<DecoderOutput::Failure> m_output_failure;
    std::array<std::atomic<std::uint32_t>, ImageCount> m_live{};
    std::atomic<std::uint32_t> m_held{0};
    std::atomic<std::uint32_t> m_held_peak{0};
    std::atomic<std::uint64_t> m_reused_while_held{0};
    std::function<void(const std::string&)> m_reporter;
    std::array<std::int64_t, ImageCount> m_slot_timestamps{};
};

}

#endif
