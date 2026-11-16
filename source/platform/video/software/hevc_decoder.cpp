#include "platform/video/software/hevc_decoder.hpp"

#include <switch.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <string>

#include <borealis.hpp>

extern "C"
{
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

namespace akira::platform::software
{

HevcDecoder::HevcDecoder() = default;

HevcDecoder::~HevcDecoder()
{
    cleanup();
}

bool HevcDecoder::initCodec(bool hevc, bool, int width, int height)
{
    m_video_width = width;
    m_video_height = height;
    if (!hevc)
    {
        brls::Logger::error("HevcDecoder: asked to decode H.264");
        return false;
    }
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!codec)
    {
        brls::Logger::error("HevcDecoder: FFmpeg has no HEVC decoder");
        return false;
    }
    m_context.reset(avcodec_alloc_context3(codec));
    if (!m_context)
        return false;
    m_context->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    m_context->flags2 |= AV_CODEC_FLAG2_FAST;
    m_context->thread_type = FF_THREAD_FRAME;
    m_context->thread_count = ThreadCount;
    if (const int rc = avcodec_open2(m_context.get(), codec, nullptr); rc < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE]{};
        av_make_error_string(error, sizeof(error), rc);
        brls::Logger::error("HevcDecoder: avcodec_open2 failed: {}", error);
        m_context.reset();
        return false;
    }
    brls::Logger::info("HevcDecoder: software HEVC at {}x{} on {} frame threads", width, height, ThreadCount);
    return true;
}

bool HevcDecoder::initVideo(int width, int height)
{
    m_video_width = width;
    m_video_height = height;
    m_waiting_for_keyframe = true;
    return m_context != nullptr;
}

void HevcDecoder::updateResolution(int width, int height)
{
    m_video_width = width;
    m_video_height = height;
}

std::vector<HevcDecoder::NalUnit> HevcDecoder::nalUnits(std::span<const std::uint8_t> sample)
{
    std::vector<std::size_t> starts;
    for (std::size_t offset = 0; offset + 3 < sample.size(); ++offset)
    {
        if (sample[offset] == 0x00 && sample[offset + 1] == 0x00 && sample[offset + 2] == 0x01)
        {
            starts.push_back(offset + 3);
            offset += 2;
        }
    }
    std::vector<NalUnit> units;
    units.reserve(starts.size());
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        const std::size_t begin = starts[index];
        std::size_t end = index + 1 < starts.size() ? starts[index + 1] - 3 : sample.size();
        while (end > begin && sample[end - 1] == 0x00)
            --end;
        if (end > begin)
            units.push_back({static_cast<std::uint8_t>((sample[begin] >> 1) & 0x3F), sample.subspan(begin, end - begin)});
    }
    return units;
}

void HevcDecoder::rememberParameterSets(std::span<const NalUnit> units)
{
    m_parameter_sets.clear();
    for (const NalUnit& unit : units | std::views::filter(&HevcDecoder::isParameterSet))
    {
        m_parameter_sets.insert(m_parameter_sets.end(), {0x00, 0x00, 0x00, 0x01});
        m_parameter_sets.insert(m_parameter_sets.end(), unit.bytes.begin(), unit.bytes.end());
    }
}

bool HevcDecoder::decode(const AVBufferHandle& data, std::size_t size)
{
    if (!m_context || !data)
        return false;

    const auto entry = Clock::now();
    publishStats(entry);
    m_last_corrupt = false;

    const std::span<const std::uint8_t> sample(data.get()->data, size);
    const auto units = nalUnits(sample);
    const bool hasParameterSets = std::ranges::any_of(units, &HevcDecoder::isParameterSet);
    if (hasParameterSets)
        rememberParameterSets(units);

    if (m_waiting_for_keyframe)
    {
        if (!std::ranges::any_of(units, &HevcDecoder::isKeyframe))
        {
            ++m_drops;
            return false;
        }
        std::string types;
        for (const NalUnit& unit : units)
            std::format_to(std::back_inserter(types), "{}{}:{}", types.empty() ? "" : " ", unit.type, unit.bytes.size());
        brls::Logger::info("HevcDecoder: keyframe of {} bytes, NAL units {}{}", size, types,
            hasParameterSets ? "" : m_parameter_sets.empty() ? ", no parameter sets yet" : ", adding the stored parameter sets");
        m_waiting_for_keyframe = false;
        if (!hasParameterSets && !sendStoredParameterSets(entry))
            return false;
    }

    return send(data, size, entry);
}

bool HevcDecoder::sendStoredParameterSets(Clock::time_point entry)
{
    if (m_parameter_sets.empty())
        return true;
    AVBufferHandle sets(m_parameter_sets.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!sets)
        return false;
    std::ranges::copy(m_parameter_sets, reinterpret_cast<std::uint8_t*>(sets.data()));
    std::fill_n(sets.data() + m_parameter_sets.size(), AV_INPUT_BUFFER_PADDING_SIZE, std::byte{0});
    return send(sets, m_parameter_sets.size(), entry);
}

bool HevcDecoder::send(const AVBufferHandle& data, std::size_t size, Clock::time_point entry)
{
    AVPacketGuard packet;
    packet->buf = av_buffer_ref(data.get());
    if (!packet->buf)
        return false;
    packet->data = data.get()->data;
    packet->size = static_cast<int>(size);

    while (true)
    {
        const int rc = avcodec_send_packet(m_context.get(), packet);
        if (rc == 0)
            return drain(entry);
        if (rc != AVERROR(EAGAIN))
        {
            char error[AV_ERROR_MAX_STRING_SIZE]{};
            av_make_error_string(error, sizeof(error), rc);
            brls::Logger::warning("HevcDecoder: avcodec_send_packet failed: {}", error);
            return false;
        }
        if (!drain(entry))
            return false;
    }
}

bool HevcDecoder::drain(Clock::time_point entry)
{
    while (true)
    {
        const int rc = avcodec_receive_frame(m_context.get(), m_decoded);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
            return true;
        if (rc < 0)
        {
            char error[AV_ERROR_MAX_STRING_SIZE]{};
            av_make_error_string(error, sizeof(error), rc);
            brls::Logger::warning("HevcDecoder: avcodec_receive_frame failed: {}", error);
            return false;
        }
        if (m_decoded->flags & AV_FRAME_FLAG_CORRUPT)
            m_last_corrupt = true;
        else
            emit(*m_decoded.frame, entry);
        av_frame_unref(m_decoded);
    }
}

bool HevcDecoder::ensurePool(std::uint32_t width, std::uint32_t height)
{
    if (m_pool && m_pool->width() == width && m_pool->height() == height)
        return true;
    auto pool = FramePool::create(width, height);
    if (!pool)
    {
        brls::Logger::error("HevcDecoder: no memory for {}x{} frames", width, height);
        m_pool.reset();
        return false;
    }
    m_pool = std::move(*pool);
    brls::Logger::info("HevcDecoder: frame pool {}x{}, {} slots of {:#x} bytes", width, height, FramePool::Slots,
        m_pool->backingSize());
    return true;
}

void HevcDecoder::emit(const AVFrame& decoded, Clock::time_point entry)
{
    const auto format = static_cast<AVPixelFormat>(decoded.format);
    if (!m_logged_format)
    {
        const char* name = av_get_pix_fmt_name(format);
        brls::Logger::info("HevcDecoder: first frame {}x{} {}", decoded.width, decoded.height, name ? name : "unknown");
        m_logged_format = true;
    }
    if (!ensurePool(static_cast<std::uint32_t>(decoded.width), static_cast<std::uint32_t>(decoded.height)))
    {
        ++m_drops;
        return;
    }
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(entry.time_since_epoch()).count();
    auto frame = m_pool->acquire(timestamp);
    if (!frame)
    {
        ++m_drops;
        return;
    }
    if (!fill(decoded, **frame, *m_pool))
    {
        ++m_drops;
        return;
    }
    m_last_emit = Clock::now();
    ++m_window_frames;
    m_window_decode_us += std::chrono::duration<double, std::micro>(m_last_emit - entry).count();
    if (m_frame_ready_callback)
        m_frame_ready_callback(std::move(*frame));
    else
        ++m_drops;
}

bool HevcDecoder::fill(const AVFrame& decoded, PooledFrame& target, const FramePool& pool)
{
    const auto format = static_cast<AVPixelFormat>(decoded.format);
    const bool tenBit = format == AV_PIX_FMT_YUV420P10LE;
    if (format != AV_PIX_FMT_YUV420P && format != AV_PIX_FMT_YUVJ420P && !tenBit)
        return false;

    const auto bytes = target.bytes();
    const auto width = static_cast<std::size_t>(decoded.width);
    const auto height = static_cast<std::size_t>(decoded.height);
    const auto chromaWidth = (width + 1) / 2;
    const auto chromaHeight = (height + 1) / 2;
    const auto pitch = std::size_t{pool.luma().pitch};
    auto* luma = reinterpret_cast<std::uint8_t*>(bytes.data()) + pool.luma().offset;
    auto* chroma = reinterpret_cast<std::uint8_t*>(bytes.data()) + pool.chroma().offset;

    if (tenBit)
    {
        for (std::size_t row = 0; row < height; ++row)
        {
            const auto* source = reinterpret_cast<const std::uint16_t*>(decoded.data[0] + row * decoded.linesize[0]);
            std::transform(source, source + width, luma + row * pitch,
                [](std::uint16_t value) { return static_cast<std::uint8_t>(value >> 2); });
        }
        for (std::size_t row = 0; row < chromaHeight; ++row)
        {
            const auto* u = reinterpret_cast<const std::uint16_t*>(decoded.data[1] + row * decoded.linesize[1]);
            const auto* v = reinterpret_cast<const std::uint16_t*>(decoded.data[2] + row * decoded.linesize[2]);
            auto* out = chroma + row * pitch;
            for (std::size_t column = 0; column < chromaWidth; ++column)
            {
                out[column * 2] = static_cast<std::uint8_t>(u[column] >> 2);
                out[column * 2 + 1] = static_cast<std::uint8_t>(v[column] >> 2);
            }
        }
    }
    else
    {
        for (std::size_t row = 0; row < height; ++row)
            std::memcpy(luma + row * pitch, decoded.data[0] + row * decoded.linesize[0], width);
        for (std::size_t row = 0; row < chromaHeight; ++row)
        {
            const auto* u = decoded.data[1] + row * decoded.linesize[1];
            const auto* v = decoded.data[2] + row * decoded.linesize[2];
            auto* out = chroma + row * pitch;
            for (std::size_t column = 0; column < chromaWidth; ++column)
            {
                out[column * 2] = u[column];
                out[column * 2 + 1] = v[column];
            }
        }
    }
    armDCacheFlush(bytes.data(), bytes.size());
    return true;
}

void HevcDecoder::publishStats(Clock::time_point now)
{
    if (m_window_start == Clock::time_point{})
    {
        m_window_start = now;
        return;
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(now - m_window_start).count();
    if (elapsedMs < 1000.0)
        return;
    {
        std::lock_guard lock(m_stats_mutex);
        if (m_window_frames > 0)
            m_stats.decode_ms = static_cast<float>(m_window_decode_us / static_cast<double>(m_window_frames) / 1000.0);
        m_stats.source_fps = static_cast<float>(static_cast<double>(m_window_frames) * 1000.0 / elapsedMs);
        m_stats.decoder_drops = m_drops;
    }
    m_window_start = now;
    m_window_frames = 0;
    m_window_decode_us = 0.0;
}

HevcDecoder::Stats HevcDecoder::getStats() const
{
    std::lock_guard lock(m_stats_mutex);
    return m_stats;
}

void HevcDecoder::flush()
{
    brls::Logger::warning("HevcDecoder: flushing, waiting for a keyframe");
    if (m_context)
        avcodec_flush_buffers(m_context.get());
    m_waiting_for_keyframe = true;
}

void HevcDecoder::cleanup()
{
    m_context.reset();
    m_pool.reset();
}

}
