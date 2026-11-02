#ifndef AKIRA_AV_WRAPPERS_HPP
#define AKIRA_AV_WRAPPERS_HPP

#include <cstddef>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
}

class AVBufferHandle {
public:
    AVBufferHandle() = default;
    explicit AVBufferHandle(std::size_t size) : buffer(av_buffer_alloc(size)) {}
    AVBufferHandle(AVBufferHandle&& other) noexcept : buffer(std::exchange(other.buffer, nullptr)) {}
    AVBufferHandle& operator=(AVBufferHandle&& other) noexcept
    {
        if (this != &other)
        {
            av_buffer_unref(&buffer);
            buffer = std::exchange(other.buffer, nullptr);
        }
        return *this;
    }
    AVBufferHandle(const AVBufferHandle&) = delete;
    AVBufferHandle& operator=(const AVBufferHandle&) = delete;
    ~AVBufferHandle() { av_buffer_unref(&buffer); }

    explicit operator bool() const { return buffer != nullptr; }
    AVBufferRef* get() const { return buffer; }
    std::byte* data() const { return reinterpret_cast<std::byte*>(buffer->data); }

private:
    AVBufferRef* buffer = nullptr;
};

struct AVPacketGuard {
    AVPacket* pkt;
    AVPacketGuard() : pkt(av_packet_alloc()) {}
    ~AVPacketGuard() { if (pkt) av_packet_free(&pkt); }
    AVPacketGuard(const AVPacketGuard&) = delete;
    AVPacketGuard& operator=(const AVPacketGuard&) = delete;
    operator AVPacket*() { return pkt; }
    AVPacket* operator->() { return pkt; }
};

struct AVFrameGuard {
    AVFrame* frame;
    AVFrameGuard() : frame(av_frame_alloc()) {}
    ~AVFrameGuard() { if (frame) av_frame_free(&frame); }
    AVFrameGuard(const AVFrameGuard&) = delete;
    AVFrameGuard& operator=(const AVFrameGuard&) = delete;
    operator AVFrame*() { return frame; }
    AVFrame* operator->() { return frame; }
    AVFrame* release() { auto* f = frame; frame = nullptr; return f; }
};

#endif // AKIRA_AV_WRAPPERS_HPP
