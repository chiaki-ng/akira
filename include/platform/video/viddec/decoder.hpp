#ifndef AKIRA_IO_VIDEO_DECODER_HPP
#define AKIRA_IO_VIDEO_DECODER_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <span>
#include <vector>
#include <chrono>
#include <mutex>
#include <optional>

#include "platform/video/viddec/hardware_decoder.hpp"
#include "util/av_wrappers.hpp"
#include "platform/video/decoder.hpp"

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::platform::viddec
{

class ViddecDecoder final : public akira::platform::VideoDecoder
{
public:
    using Frame = akira::platform::viddec::HardwareDecoder::Frame;
    using FrameReadyCallback = std::function<void(Frame)>;

    explicit ViddecDecoder(const settings::DeveloperSettings& developer);
    ~ViddecDecoder() override;

    // Disable copy
    ViddecDecoder(const ViddecDecoder&) = delete;
    ViddecDecoder& operator=(const ViddecDecoder&) = delete;

    void setFrameReadyCallback(FrameReadyCallback callback) { m_frame_ready_callback = std::move(callback); }
    void setOutputMemory(std::shared_ptr<akira::platform::viddec::OutputMemory> memory) { m_output_memory = std::move(memory); }

    bool initCodec(bool hevc, bool ps5, int width, int height) override;

    // Initialize video with frame queue
    bool initVideo(int video_width, int video_height) override;

    // Decode a video packet (returns false if waiting for keyframe)
    bool decode(const AVBufferHandle& data, std::size_t size) override;

    // Flush decoder buffers and set waiting-for-keyframe state
    void flush() override;
    void stalled(std::uint32_t episode) override;

    // Cleanup
    void cleanup() override;

    int getVideoWidth() const override { return m_video_width; }
    int getVideoHeight() const override { return m_video_height; }
    void updateResolution(int width, int height) override { m_video_width = width; m_video_height = height; }
    bool isHEVC() const override { return m_is_hevc; }
    bool isHardwareAccelerated() const override { return true; }

    using DecoderStats = akira::platform::VideoDecoder::Stats;

    DecoderStats getStats() const override;
    std::chrono::steady_clock::time_point lastEmitTime() const override { return m_last_emit; }
    bool lastDecodeCorrupt() const override { return false; }

private:
    const settings::DeveloperSettings& m_developer;
    static constexpr std::uint8_t NalSlice = 1;
    static constexpr std::uint8_t NalIdr = 5;
    static constexpr std::uint8_t NalSps = 7;
    static constexpr std::uint8_t NalPps = 8;
    static constexpr std::array<std::uint8_t, 4> StartCode{0x00, 0x00, 0x00, 0x01};
    static constexpr std::uint32_t Ps4ReferenceLimit = 3;
    static constexpr std::uint32_t Ps5ReferenceLimit = 5;
    static constexpr std::chrono::milliseconds SubmitTimeout{50};
    static constexpr std::chrono::milliseconds FirstFrameWait{20};
    static constexpr std::uint32_t ZeroCopyStalls = 3;

    struct NalUnit
    {
        std::uint8_t type;
        std::span<const std::uint8_t> bytes;
    };

    struct Keyframe
    {
        bool idr = false;
        bool parameters_changed = false;
        bool has_sps = false;
        bool has_slices = false;
    };

    static std::vector<NalUnit> splitNalUnits(std::span<const std::uint8_t> sample);
    Keyframe scanNalUnits(std::span<const std::uint8_t> sample);
    std::vector<std::uint8_t> codecConfig() const;
    std::vector<std::uint8_t> decoderSample(std::span<const std::uint8_t> sample) const;
    void rememberDecoderSps();
    bool ensureHardwareDecoder(const Keyframe& keyframe);
    void emitReadyFrames(std::chrono::steady_clock::time_point decode_entry, std::chrono::nanoseconds first_wait);

    void noteDecodeSample(std::chrono::steady_clock::duration d);
    void notePacketArrival(std::chrono::steady_clock::time_point now);
    void publishStats(std::chrono::steady_clock::time_point now);

    mutable std::mutex m_stats_mutex;
    DecoderStats m_stats;
    uint64_t m_drops = 0;
    std::chrono::steady_clock::time_point m_last_emit{};
    std::chrono::steady_clock::time_point m_last_packet{};
    std::chrono::steady_clock::time_point m_window_start{};
    double m_decode_us_accum = 0.0;
    uint64_t m_decode_sample_count = 0;
    uint64_t m_frames_emitted = 0;
    uint64_t m_window_submitted = 0;
    uint64_t m_window_failed = 0;
    double m_interval_us_accum = 0.0;
    double m_interval_dev_accum = 0.0;
    uint64_t m_interval_count = 0;
    double m_interval_mean_us = 0.0;

    std::shared_ptr<akira::platform::viddec::HardwareDecoder> m_hardware;
    std::shared_ptr<akira::platform::viddec::OutputMemory> m_output_memory;
    std::vector<std::uint8_t> m_sps;
    std::vector<std::uint8_t> m_decoder_sps;
    std::vector<std::uint8_t> m_pps;

    FrameReadyCallback m_frame_ready_callback;

    int m_video_width = 0;
    int m_video_height = 0;
    bool m_is_hevc = false;
    std::uint32_t m_reference_limit = Ps4ReferenceLimit;
    bool m_waiting_for_idr = false;
};


}

#endif // AKIRA_IO_VIDEO_DECODER_HPP
