#ifndef AKIRA_IO_VIDEO_RENDERER_HPP
#define AKIRA_IO_VIDEO_RENDERER_HPP

#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>
#include "stream/stream_stats.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
}

// Abstract interface for video rendering
// Implementations: OpenGLRenderer, Deko3dRenderer
class IVideoRenderer
{
public:
    virtual ~IVideoRenderer() = default;

    virtual bool initialize(int frame_width, int frame_height) = 0;

    virtual bool isInitialized() const = 0;

    virtual void draw(AVFrame* frame) = 0;

    virtual void presentFrame(AVFrame* frame) { draw(frame); }

    virtual void cleanup() = 0;

    virtual void waitIdle() {}


    virtual void updateOverlayTexture() {}

    virtual void setStatsOverlayMode(StatsOverlayMode mode) { m_stats_mode.store(mode, std::memory_order_relaxed); }
    virtual void setStreamStats(const StreamStats& stats)
    {
        m_stats_seq.fetch_add(1, std::memory_order_release);
        m_stats = stats;
        m_stats_seq.fetch_add(1, std::memory_order_release);
    }

    StreamStats readStreamStats() const
    {
        for (;;)
        {
            uint32_t before = m_stats_seq.load(std::memory_order_acquire);
            if (before & 1u)
                continue;
            StreamStats copy = m_stats;
            if (m_stats_seq.load(std::memory_order_acquire) == before)
                return copy;
        }
    }
    virtual float getRenderFPS() const { return 0.0f; }
    virtual void setPaused(bool paused) { (void)paused; }
    virtual void setConnectionContext(const std::string& context) { (void)context; }
    virtual void updateResolution(int width, int height) { (void)width; (void)height; }
    virtual void triggerBorderFlash() {}

    virtual bool captureLastFrame(std::vector<uint8_t>& rgba, int& width, int& height)
    {
        (void)rgba; (void)width; (void)height;
        return false;
    }

    virtual bool overlayTouchBegin(float x, float y) { (void)x; (void)y; return false; }
    virtual void overlayTouchMove(float x, float y) { (void)x; (void)y; }
    virtual void overlayTouchEnd() {}
    virtual bool takeOverlayPositionDirty() { return false; }

    using TickCallback = std::function<bool()>;
    virtual void setTickCallback(TickCallback cb) { (void)cb; }

protected:
    std::atomic<StatsOverlayMode> m_stats_mode{StatsOverlayMode::Off};
    StreamStats m_stats;
    mutable std::atomic<uint32_t> m_stats_seq{0};
};

#endif // AKIRA_IO_VIDEO_RENDERER_HPP
