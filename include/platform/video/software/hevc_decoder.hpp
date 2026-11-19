#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "platform/video/decoder.hpp"
#include "platform/video/nv12_frame.hpp"
#include "platform/video/software/frame_pool.hpp"
#include "util/av_wrappers.hpp"

namespace brls
{

class Box;

}

namespace akira::platform::software
{

class HevcDecoder final : public akira::platform::VideoDecoder
{
public:
    using Frame = std::shared_ptr<const akira::platform::Nv12Frame>;
    using FrameReadyCallback = std::function<void(Frame)>;

    HevcDecoder();
    ~HevcDecoder() override;
    HevcDecoder(const HevcDecoder&) = delete;
    HevcDecoder& operator=(const HevcDecoder&) = delete;

    static void addSetting(brls::Box& group);

    void setFrameReadyCallback(FrameReadyCallback callback) { m_frame_ready_callback = std::move(callback); }

    bool initCodec(bool hevc, bool ps5, int width, int height) override;
    bool initVideo(int width, int height) override;
    bool decode(const AVBufferHandle& data, std::size_t size) override;
    void flush() override;
    void cleanup() override;
    void updateResolution(int width, int height) override;

    int getVideoWidth() const override { return m_video_width; }
    int getVideoHeight() const override { return m_video_height; }
    bool isHEVC() const override { return true; }
    bool isHardwareAccelerated() const override { return false; }
    Stats getStats() const override;
    std::chrono::steady_clock::time_point lastEmitTime() const override { return m_last_emit; }
    bool lastDecodeCorrupt() const override { return m_last_corrupt; }

private:
    using Clock = std::chrono::steady_clock;

    static constexpr int ThreadCount = 3;
    static constexpr std::uint8_t NalIdrWithLeading = 19;
    static constexpr std::uint8_t NalCra = 21;
    static constexpr std::uint8_t NalVps = 32;
    static constexpr std::uint8_t NalPps = 34;

    struct NalUnit
    {
        std::uint8_t type;
        std::span<const std::uint8_t> bytes;
    };

    struct ContextFree
    {
        void operator()(AVCodecContext* context) const { avcodec_free_context(&context); }
    };

    static std::vector<NalUnit> nalUnits(std::span<const std::uint8_t> sample);
    static bool isKeyframe(const NalUnit& unit) { return unit.type >= NalIdrWithLeading && unit.type <= NalCra; }
    static bool isParameterSet(const NalUnit& unit) { return unit.type >= NalVps && unit.type <= NalPps; }
    void rememberParameterSets(std::span<const NalUnit> units);
    bool sendStoredParameterSets(Clock::time_point entry);
    bool send(const AVBufferHandle& data, std::size_t size, Clock::time_point entry);
    bool drain(Clock::time_point entry);
    void emit(const AVFrame& decoded, Clock::time_point entry);
    bool ensurePool(std::uint32_t width, std::uint32_t height);
    static bool fill(const AVFrame& decoded, PooledFrame& target, const FramePool& pool);
    void publishStats(Clock::time_point now);

    std::unique_ptr<AVCodecContext, ContextFree> m_context;
    AVFrameGuard m_decoded;
    std::shared_ptr<FramePool> m_pool;
    std::vector<std::uint8_t> m_parameter_sets;
    FrameReadyCallback m_frame_ready_callback;

    int m_video_width = 0;
    int m_video_height = 0;
    bool m_waiting_for_keyframe = true;
    bool m_last_corrupt = false;
    bool m_logged_format = false;
    Clock::time_point m_last_emit{};

    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    Clock::time_point m_window_start{};
    std::uint64_t m_window_frames = 0;
    double m_window_decode_us = 0.0;
    std::uint64_t m_drops = 0;
};

}
