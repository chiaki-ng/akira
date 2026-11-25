#include "platform/video/nvtegra/decoder.hpp"
#include "log/log.hpp"

#include <bit>
#include <chrono>
#include <cstring>
#include <memory>
#include "app/exception.hpp"
#include "util/av_wrappers.hpp"

#ifdef BOREALIS_USE_DEKO3D
extern "C"
{
#include <libavutil/hwcontext_nvtegra.h>
}
#endif

namespace akira::platform::nvtegra
{

NvtegraDecoder::NvtegraDecoder()
{
}

NvtegraDecoder::~NvtegraDecoder()
{
    cleanup();
}

std::optional<deko3d::VideoFrame> NvtegraDecoder::adoptFrame(AVFrame* frame)
{
    std::shared_ptr<AVFrame> owned(frame, [](AVFrame* f) { av_frame_free(&f); });
#ifdef BOREALIS_USE_DEKO3D
    if (frame->format != AV_PIX_FMT_NVTEGRA || !frame->data[0] || !frame->buf[0])
        return std::nullopt;

    AVNVTegraMap* map = av_nvtegra_frame_get_fbuf_map(frame);
    if (!map)
        return std::nullopt;

    void* storage = av_nvtegra_map_get_addr(map);
    const std::size_t size = av_nvtegra_map_get_size(map);
    return deko3d::VideoFrame{
        .width = frame->width,
        .height = frame->height,
        .storage = storage,
        .storageSize = size,
        .chromaOffset = static_cast<std::size_t>(frame->data[1] - frame->data[0]),
        .lumaPitch = static_cast<std::uint32_t>(frame->linesize[0]),
        .chromaPitch = static_cast<std::uint32_t>(frame->linesize[1]),
        .layout = deko3d::PlaneLayout::BlockLinear,
        .blockHeightLog2 = NvdecBlockHeightLog2,
        .cacheKey = av_nvtegra_map_get_handle(map),
        .lifetime = owned,
        .prepareCpuRead = [map, storage, size] { av_nvtegra_map_cache_op(map, NVMAP_CACHE_OP_INV, storage, size); },
    };
#else
    return std::nullopt;
#endif
}

bool NvtegraDecoder::initCodec(bool hevc, [[maybe_unused]] bool ps5, int width, int height)
{
    log::info("NvtegraDecoder: loading AVCodec");

    m_is_hevc = hevc;
    if (hevc)
    {
        m_codec = avcodec_find_decoder_by_name("hevc");
    }
    else
    {
        m_codec = avcodec_find_decoder_by_name("h264");
    }

    if (!m_codec)
    {
        throw app::Exception("Codec not available");
    }

    log::info("NvtegraDecoder: got codec {}", m_codec->name);

    m_codec_context = avcodec_alloc_context3(m_codec);
    if (!m_codec_context)
    {
        throw app::Exception("Failed to alloc codec context");
    }

    if (m_hw_accel_enabled)
    {
        m_codec_context->skip_loop_filter = AVDISCARD_ALL;
        m_codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY | AV_CODEC_FLAG_OUTPUT_CORRUPT;
        m_codec_context->flags2 |= AV_CODEC_FLAG2_FAST;
        m_codec_context->thread_type = FF_THREAD_SLICE;
        m_codec_context->thread_count = 1;
        m_codec_context->has_b_frames = 0;
#ifdef BOREALIS_USE_DEKO3D
        // Create HW device context BEFORE opening codec 
	log::info("NvtegraDecoder: Creating NVTegra hardware device context...");
        if (av_hwdevice_ctx_create(&m_hw_device_ctx, AV_HWDEVICE_TYPE_NVTEGRA, NULL, NULL, 0) < 0)
        {
            throw app::Exception("Failed to create NVTegra hardware device context");
        }
        m_codec_context->hw_device_ctx = av_buffer_ref(m_hw_device_ctx);
        log::info("NvtegraDecoder: NVTegra hw device context created: {}", (void*)m_hw_device_ctx);

        if (width > 0 && height > 0)
        {
            m_codec_context->width = width;
            m_codec_context->height = height;
            m_codec_context->pix_fmt = AV_PIX_FMT_NVTEGRA;
            log::info("NvtegraDecoder: Setting NVTEGRA format {}x{}", width, height);
        }
#endif
    }
    else
    {
        m_codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY | AV_CODEC_FLAG_OUTPUT_CORRUPT;
        m_codec_context->flags2 |= AV_CODEC_FLAG2_FAST;
        m_codec_context->thread_type = FF_THREAD_SLICE;
        m_codec_context->thread_count = 4;
    }

    // Open codec AFTER hw_device_ctx is attached (so FFmpeg knows to use HW acceleration)
    if (avcodec_open2(m_codec_context, m_codec, nullptr) < 0)
    {
        avcodec_free_context(&m_codec_context);
        throw app::Exception("Failed to open codec context");
    }

    return true;
}

bool NvtegraDecoder::initVideo(int video_width, int video_height)
{
    log::info("NvtegraDecoder: initVideo {}x{}", video_width, video_height);

    m_video_width = video_width;
    m_video_height = video_height;

    // Wait for IDR frame at session start
    // PS5 may resume with P-frames; returning false triggers CORRUPTFRAME → keyframe request
    m_waiting_for_idr = true;
    log::info("NvtegraDecoder: Waiting for IDR frame to start decoding");

    m_tmp_frame = av_frame_alloc();
    if (!m_tmp_frame)
    {
        log::error("NvtegraDecoder: Failed to allocate temp frame");
        return false;
    }

    return true;
}

void NvtegraDecoder::noteDecodeSample(std::chrono::steady_clock::duration d)
{
    m_decode_us_accum += (double)std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    m_decode_sample_count++;
}

void NvtegraDecoder::notePacketArrival(std::chrono::steady_clock::time_point now)
{
    if (m_last_packet != std::chrono::steady_clock::time_point{})
    {
        double interval_us = (double)std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_packet).count();
        if (interval_us > 0.0 && interval_us < 1000000.0)
        {
            m_interval_us_accum += interval_us;
            if (m_interval_mean_us > 0.0)
            {
                double dev = interval_us - m_interval_mean_us;
                m_interval_dev_accum += dev < 0.0 ? -dev : dev;
            }
            m_interval_count++;
        }
    }
    m_last_packet = now;
}

NvtegraDecoder::DecoderStats NvtegraDecoder::getStats() const
{
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void NvtegraDecoder::publishStats(std::chrono::steady_clock::time_point now)
{
    if (m_window_start == std::chrono::steady_clock::time_point{})
    {
        m_window_start = now;
        return;
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_window_start).count();
    if (elapsed_ms < 1000)
        return;

    if (m_interval_count > 0)
        m_interval_mean_us = m_interval_us_accum / (double)m_interval_count;

    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        if (m_decode_sample_count > 0)
            m_stats.decode_ms = (float)(m_decode_us_accum / (double)m_decode_sample_count / 1000.0);

        m_stats.source_fps = (float)((double)m_frames_emitted * 1000.0 / (double)elapsed_ms);

        if (m_interval_count > 0)
            m_stats.jitter_ms = (float)(m_interval_dev_accum / (double)m_interval_count / 1000.0);

        m_stats.decoder_drops = m_drops;
    }

    m_window_start = now;
    m_decode_us_accum = 0.0;
    m_decode_sample_count = 0;
    m_frames_emitted = 0;
    m_interval_us_accum = 0.0;
    m_interval_dev_accum = 0.0;
    m_interval_count = 0;
}

bool NvtegraDecoder::decode(const AVBufferHandle& data, std::size_t size)
{
    const uint8_t* buf = data.get()->data;
    const size_t buf_size = size;

    m_corrupt_in_call = false;
    auto decode_entry = std::chrono::steady_clock::now();
    notePacketArrival(decode_entry);
    publishStats(decode_entry);

    bool has_idr = scanNALUnits(buf, buf_size);

    bool has_all_params = m_is_hevc ? (m_has_vps && m_has_sps && m_has_pps)
                                     : (m_has_sps && m_has_pps);

    if (m_waiting_for_idr)
    {
        if (has_all_params && has_idr)
        {
            log::info("NvtegraDecoder: Got complete keyframe (params + IDR), resuming decode");
            m_waiting_for_idr = false;
        }
        else
        {
            static uint32_t idr_wait_count = 0;
            if (idr_wait_count++ % 10 == 0)
            {
                if (m_is_hevc)
                {
                    log::info("NvtegraDecoder: Waiting for keyframe (VPS={}, SPS={}, PPS={}, IDR={}) skip #{}",
                        m_has_vps, m_has_sps, m_has_pps, has_idr, idr_wait_count);
                }
                else
                {
                    log::info("NvtegraDecoder: Waiting for keyframe (SPS={}, PPS={}, IDR={}) skip #{}",
                        m_has_sps, m_has_pps, has_idr, idr_wait_count);
                }
            }
        }
    }

    // Fully drain all frames the decoder has already produced.
    // FFmpeg may output multiple frames for one packet, and send_packet(EAGAIN)
    // means we must receive pending frames before retrying the send.
    auto drain_frames = [this, decode_entry]() -> bool {
        while (true)
        {
            int receive_result = avcodec_receive_frame(m_codec_context, m_tmp_frame);
            if (receive_result == 0)
            {
                if (m_tmp_frame->flags & AV_FRAME_FLAG_CORRUPT)
                {
                    m_corrupt_in_call = true;
                    if (std::has_single_bit(++m_corrupt_frames))
                        log::warning("NvtegraDecoder: held a frame decoded from a missing reference ({} so far)",
                            m_corrupt_frames);
                    av_frame_unref(m_tmp_frame);
                    continue;
                }
                AVFrame* queued_frame = av_frame_alloc();
                if (!queued_frame)
                {
                    log::error("NvtegraDecoder: Failed to allocate queued frame");
                    return false;
                }

                av_frame_move_ref(queued_frame, m_tmp_frame);
                m_last_emit = std::chrono::steady_clock::now();
                noteDecodeSample(m_last_emit - decode_entry);
                m_frames_emitted++;
                if (m_frame_ready_callback)
                {
                    if (auto frame = adoptFrame(queued_frame))
                        m_frame_ready_callback(*frame);
                }
                else
                {
                    m_drops++;
                    av_frame_free(&queued_frame);
                }
                continue;
            }

            if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF)
                return true;

            char errbuf[128];
            av_make_error_string(errbuf, sizeof(errbuf), receive_result);
            log::warning("NvtegraDecoder: avcodec_receive_frame failed: {}", errbuf);
            return false;
        }
    };

    AVPacketGuard packet;
    packet->buf = av_buffer_ref(data.get());
    if (!packet->buf)
        return false;
    packet->data = data.get()->data;
    packet->size = static_cast<int>(buf_size);

    while (true)
    {
        int r = avcodec_send_packet(m_codec_context, packet);
        if (r == 0)
            return drain_frames();

        if (r != AVERROR(EAGAIN))
        {
            char errbuf[128];
            av_make_error_string(errbuf, sizeof(errbuf), r);
            log::error("Failed to push frame: {}", errbuf);
            return false;
        }

        if (!drain_frames())
            return false;
    }
}

void NvtegraDecoder::flush()
{
    if (m_codec_context)
    {
        log::warning("NvtegraDecoder: Flushing decoder, waiting for IDR");
        avcodec_flush_buffers(m_codec_context);
        m_waiting_for_idr = true;
    }
}

bool NvtegraDecoder::scanNALUnits(const uint8_t* buf, size_t buf_size)
{
    if (buf_size < 5)
        return false;

    bool has_idr = false;
    bool prev_vps = m_has_vps, prev_sps = m_has_sps, prev_pps = m_has_pps;

    size_t offset = 0;
    while (offset + 4 < buf_size)
    {
        if (buf[offset] == 0x00 && buf[offset + 1] == 0x00)
        {
            if (buf[offset + 2] == 0x01)
                offset += 3;
            else if (buf[offset + 2] == 0x00 && offset + 3 < buf_size && buf[offset + 3] == 0x01)
                offset += 4;
            else
            {
                offset++;
                continue;
            }

            if (offset >= buf_size)
                break;

            if (m_is_hevc)
            {
                uint8_t nal_type = (buf[offset] >> 1) & 0x3F;
                if (nal_type == 32) m_has_vps = true;
                else if (nal_type == 33) m_has_sps = true;
                else if (nal_type == 34) m_has_pps = true;
                else if (nal_type == 19 || nal_type == 20) has_idr = true;
                if (nal_type < 32)
                    break;
            }
            else
            {
                uint8_t nal_type = buf[offset] & 0x1F;
                if (nal_type == 7) m_has_sps = true;
                else if (nal_type == 8) m_has_pps = true;
                else if (nal_type == 5) has_idr = true;
                if (nal_type >= 1 && nal_type <= 5)
                    break;
            }
        }
        else
        {
            offset++;
        }
    }

    if (m_is_hevc)
    {
        if (!prev_vps && m_has_vps) log::info("NvtegraDecoder: Received VPS");
        if (!prev_sps && m_has_sps) log::info("NvtegraDecoder: Received SPS");
        if (!prev_pps && m_has_pps) log::info("NvtegraDecoder: Received PPS");
    }
    else
    {
        if (!prev_sps && m_has_sps) log::info("NvtegraDecoder: Received SPS");
        if (!prev_pps && m_has_pps) log::info("NvtegraDecoder: Received PPS");
    }

    return has_idr;
}

void NvtegraDecoder::cleanup()
{
    if (m_tmp_frame)
    {
        av_frame_free(&m_tmp_frame);
        m_tmp_frame = nullptr;
    }

    if (m_hw_device_ctx)
    {
        av_buffer_unref(&m_hw_device_ctx);
        m_hw_device_ctx = nullptr;
    }

    if (m_codec_context)
    {
        avcodec_free_context(&m_codec_context);
        m_codec_context = nullptr;
    }
}

}

