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

#include "video/vulkan/hardware_decoder.hpp"
#include "util/av_wrappers.hpp"
#include "video/decoder.hpp"

class StreamDecoder final : public akira::video::Decoder
{
public:
    using Frame = akira::stream::HardwareDecoder::Frame;
    using FrameReadyCallback = std::function<void(Frame)>;

    StreamDecoder();
    ~StreamDecoder() override;

    // Disable copy
    StreamDecoder(const StreamDecoder&) = delete;
    StreamDecoder& operator=(const StreamDecoder&) = delete;

    void setFrameReadyCallback(FrameReadyCallback callback) { m_frame_ready_callback = std::move(callback); }
    void setOutputDevice(std::optional<akira::stream::DecoderOutput::Device> device) { m_output_device = device; }

    bool initCodec(bool hevc, bool ps5, int width, int height) override;

    // Initialize video with frame queue
    bool initVideo(int video_width, int video_height) override;

    // Decode a video packet (returns false if waiting for keyframe)
    bool decode(const AVBufferHandle& data, std::size_t size) override;

    // Flush decoder buffers and set waiting-for-keyframe state
    void flush() override;

    // Cleanup
    void cleanup() override;

    int getVideoWidth() const override { return m_video_width; }
    int getVideoHeight() const override { return m_video_height; }
    void updateResolution(int width, int height) override { m_video_width = width; m_video_height = height; }
    bool isHEVC() const override { return m_is_hevc; }
    bool isHardwareAccelerated() const override { return true; }

    using DecoderStats = akira::video::Decoder::Stats;

    DecoderStats getStats() const override;
    std::chrono::steady_clock::time_point lastEmitTime() const override { return m_last_emit; }
    bool lastDecodeCorrupt() const override { return false; }

private:
    static constexpr std::uint8_t NalSlice = 1;
    static constexpr std::uint8_t NalIdr = 5;
    static constexpr std::uint8_t NalSps = 7;
    static constexpr std::uint8_t NalPps = 8;
    static constexpr std::array<std::uint8_t, 4> StartCode{0x00, 0x00, 0x00, 0x01};
    static constexpr std::uint32_t Ps4ReferenceLimit = 3;
    static constexpr std::uint32_t Ps5ReferenceLimit = 5;
    static constexpr std::chrono::milliseconds SubmitTimeout{50};
    static constexpr std::chrono::milliseconds FirstFrameWait{20};

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

    std::shared_ptr<akira::stream::HardwareDecoder> m_hardware;
    std::optional<akira::stream::DecoderOutput::Device> m_output_device;
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

#endif // AKIRA_IO_VIDEO_DECODER_HPP
