#include "platform/video/viddec/hardware_decoder.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <cstdlib>
#include <atomic>
#include <format>
#include <iterator>
#include <ranges>

namespace akira::platform::viddec
{

HardwareFrame::HardwareFrame(std::shared_ptr<HardwareDecoder> owner, const VideoFrame& frame)
    : m_owner(std::move(owner)), m_frame(frame)
{
    if (m_owner->m_live[m_frame.slot].fetch_add(1, std::memory_order_relaxed) != 0)
        m_owner->m_reused_while_held.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t held = m_owner->m_held.fetch_add(1, std::memory_order_relaxed) + 1;
    std::uint32_t peak = m_owner->m_held_peak.load(std::memory_order_relaxed);
    while (held > peak && !m_owner->m_held_peak.compare_exchange_weak(peak, held, std::memory_order_relaxed))
    {
    }
}

HardwareFrame::~HardwareFrame()
{
    m_owner->m_live[m_frame.slot].fetch_sub(1, std::memory_order_relaxed);
    m_owner->m_held.fetch_sub(1, std::memory_order_relaxed);
    m_owner->returnFrame(m_frame);
}

std::uint64_t HardwareFrame::generation() const
{
    return m_owner->generation();
}

const void* HardwareFrame::backing() const
{
    return m_owner->backing(m_frame.slot);
}

std::size_t HardwareFrame::backingSize() const
{
    return m_owner->backingSize(m_frame.slot);
}

akira::platform::Nv12Plane HardwareFrame::luma() const
{
    auto* base = static_cast<const std::uint8_t*>(backing());
    return {static_cast<std::size_t>(m_frame.planes[0] - base), m_frame.pitches[0]};
}

akira::platform::Nv12Plane HardwareFrame::chroma() const
{
    auto* base = static_cast<const std::uint8_t*>(backing());
    return {static_cast<std::size_t>(m_frame.planes[1] - base), m_frame.pitches[1]};
}

std::optional<OutputHandle> HardwareFrame::output() const
{
    return m_owner->output(m_frame.slot);
}

HardwareDecoder::HardwareDecoder(std::uint32_t width, std::uint32_t height)
    : m_width(width), m_height(height)
{
    m_generation = akira::platform::Nv12Frame::nextGeneration();
    m_returned.reserve(ImageCount);
}

class HardwareDecoder::ViddecService
{
public:
    ViddecService()
        : m_result(viddecInitialize())
    {
        if (R_FAILED(m_result))
            log::warning("HardwareDecoder: viddec did not open: {:#x}", m_result);
    }

    ~ViddecService()
    {
        if (R_SUCCEEDED(m_result))
            viddecExit();
    }

    ViddecService(const ViddecService&) = delete;
    ViddecService& operator=(const ViddecService&) = delete;

private:
    Result m_result;
};

std::expected<std::shared_ptr<HardwareDecoder>, Result> HardwareDecoder::create(const Config& config)
{
    static const ViddecService service;
    std::shared_ptr<HardwareDecoder> decoder(new HardwareDecoder(config.width, config.height));
    if (config.output)
    {
        if (auto opened = decoder->openWithOutputs(config, *config.output); opened)
            return decoder;
        else
        {
            decoder->m_output_failure = opened.error();
            log::warning("HardwareDecoder: zero-copy open failed at {}: {:#x}", opened.error().step,
                opened.error().code);
        }
        decoder->closeDecoder();
    }
    const VideoDecoderConfig native{
        .codec = config.codec,
        .width = config.width,
        .height = config.height,
        .image_count = ImageCount,
        .frame_rate = 0,
        .tiling = ViddecTilingMode_PitchLinear,
        .color_policy = 0,
        .max_sample_size = 0,
        .codec_config = config.codecConfig.data(),
        .codec_config_size = config.codecConfig.size(),
    };
    if (const Result rc = videoDecoderCreate(&decoder->m_decoder, &native); R_FAILED(rc))
        return std::unexpected(rc);
    return decoder;
}

HardwareDecoder::~HardwareDecoder()
{
    closeDecoder();
}

void HardwareDecoder::closeDecoder()
{
    if (!m_outputs.empty())
        m_decoder.image_count = 0;
    videoDecoderClose(&m_decoder);
    m_outputs.clear();
    log::info("HardwareDecoder: closed the {}x{} decoder", m_width, m_height);
}

std::expected<void, OutputFailure> HardwareDecoder::openWithOutputs(const Config& config, OutputMemory& memory)
{
    ::VideoDecoder& d = m_decoder;
    const std::uint32_t width = (m_width + 1) & ~1u;
    const std::uint32_t height = (m_height + 1) & ~1u;
    d.tiling = ViddecTilingMode_PitchLinear;
    d.work_size = workSize(width, height);

    const auto failed = [](std::string_view step, Result rc) { return std::unexpected(OutputFailure{step, rc}); };
    if (const Result rc = gsvInitialize(); R_FAILED(rc))
        return failed("gsvInitialize", rc);
    d.gsv_initialized = true;
    if (const Result rc = nvInitialize(); R_FAILED(rc))
        return failed("nvInitialize", rc);
    d.nv_initialized = true;
    if (const Result rc = gsvOpenSync(&d.sync); R_FAILED(rc))
        return failed("gsvOpenSync", rc);
    d.sync_open = true;
    if (const Result rc = viddecInitialize(); R_FAILED(rc))
        return failed("viddecInitialize", rc);
    d.viddec_initialized = true;

    GsvDecodeImagePreference preference{};
    if (const Result rc = viddecMakeImagePreference(&preference, width, height, ViddecOutputFormat_Nv12, d.tiling); R_FAILED(rc))
        return failed("viddecMakeImagePreference", rc);
    for (std::uint32_t slot = 0; slot < ImageCount; ++slot)
    {
        auto output = OutputImage::acquire(memory, preference);
        if (!output)
            return std::unexpected(output.error());
        d.images[slot] = (*output)->image();
        m_outputs.push_back(std::move(output).value());
    }

    d.work = std::aligned_alloc(WorkAlignment, d.work_size);
    if (!d.work)
        return failed("aligned_alloc (work)", 0);
    armDCacheFlush(d.work, d.work_size);
    if (const Result rc = viddecOpenDecoder(&d.decoder); R_FAILED(rc))
        return failed("viddecOpenDecoder", rc);
    d.decoder_open = true;
    const std::uint64_t pixels = std::uint64_t{width} * height;
    const ViddecDecoderConfig decoderConfig{
        .codec = config.codec,
        .input_format = width >= LargePolicyWidth || height >= LargePolicyHeight ? ColorPolicyLarge : ColorPolicySmall,
        .output_format = ViddecOutputFormat_Nv12,
        .output_mode = OutputMode,
        .width = width,
        .height = height,
        .frame_rate = static_cast<std::uint32_t>(std::min<std::uint64_t>(VIDEO_DECODER_PIXEL_RATE / pixels, VIDEO_DECODER_MAX_FRAME_RATE)),
        .max_input_size = pixels * 3 / 2,
    };
    if (const Result rc = viddecDecoderInitialize(&d.decoder, &decoderConfig, d.work, d.work_size, config.codecConfig.data(),
            config.codecConfig.size());
        R_FAILED(rc))
        return failed("viddecDecoderInitialize", rc);
    for (std::uint32_t slot = 0; slot < ImageCount; ++slot)
        if (const Result rc = viddecDecoderRegisterImage(&d.decoder, slot, d.images[slot].pool.base_image_id); R_FAILED(rc))
            return failed("viddecDecoderRegisterImage", rc);
    if (R_SUCCEEDED(gsvSyncAllocateSlot(&d.sync, &d.event_slot)))
    {
        if (R_SUCCEEDED(gsvSyncCreateEvent(&d.sync, d.event_slot, &d.event, false)))
            d.event_open = true;
        else
            gsvSyncFreeSlot(&d.sync, d.event_slot);
    }
    if (const Result rc = viddecDecoderStart(&d.decoder); R_FAILED(rc))
        return failed("viddecDecoderStart", rc);
    d.started = true;
    return {};
}

std::uint64_t HardwareDecoder::workSize(std::uint32_t width, std::uint32_t height)
{
    const auto tier = std::ranges::find_if(WorkTiers, [&](const WorkTier& t) { return width <= t.width && height <= t.height; });
    const std::uint64_t mib = tier != WorkTiers.end() ? tier->mib : WorkTiers.back().mib;
    return ((mib << 20) + WorkSizeAlignment - 1) & ~(WorkSizeAlignment - 1);
}

bool HardwareDecoder::timedOut(Result rc)
{
    return rc == KERNELRESULT(TimedOut);
}

std::expected<void, Result> HardwareDecoder::submit(std::span<const std::uint8_t> sample, std::int64_t timestampNs,
    std::chrono::nanoseconds timeout)
{
    constexpr auto slice = std::chrono::milliseconds(2);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;)
    {
        releaseReturnedFrames();
        const auto remaining = std::max(std::chrono::nanoseconds(0),
            std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - std::chrono::steady_clock::now()));
        const auto wait = std::min<std::chrono::nanoseconds>(remaining, slice);
        const Result rc = videoDecoderSubmit(&m_decoder, sample.data(), sample.size(), timestampNs, false,
            static_cast<u64>(wait.count()));
        if (R_SUCCEEDED(rc))
            return {};
        if (!timedOut(rc) || remaining <= wait)
            return std::unexpected(rc);
    }
}

std::optional<HardwareDecoder::Frame> HardwareDecoder::nextFrame(std::chrono::nanoseconds timeout)
{
    releaseReturnedFrames();
    VideoFrame frame{};
    if (const Result rc = videoDecoderGetFrame(&m_decoder, &frame, static_cast<u64>(timeout.count())); R_FAILED(rc))
    {
        if (!timedOut(rc) && m_reporter)
            m_reporter(std::format("videoDecoderGetFrame 0x{:x}, {}", rc, queueState()));
        return std::nullopt;
    }
    if (frame.slot < ImageCount)
    {
        std::int64_t& previous = m_slot_timestamps[frame.slot];
        if (frame.timestamp_ns <= previous && m_reporter)
            m_reporter(std::format("slot {} came back with timestamp {} after {}, {} held in slot, {}", frame.slot, frame.timestamp_ns,
                previous, m_live[frame.slot].load(std::memory_order_relaxed), queueState()));
        previous = std::max(previous, frame.timestamp_ns);
    }
    return std::make_shared<const HardwareFrame>(shared_from_this(), frame);
}

std::string HardwareDecoder::queueState() const
{
    const ViddecDecoder& session = m_decoder.decoder;
    std::string queued;
    for (std::uint32_t i = 0; i < std::min<std::uint32_t>(session.queue_count, ImageCount); ++i)
        std::format_to(std::back_inserter(queued), "{}{}", i ? "," : "", session.queue[(session.queue_head + i) % ImageCount]);
    return std::format("queue {} [{}] pending {} slot {}", session.queue_count, queued, m_decoder.pending, m_decoder.pending_slot);
}

std::expected<void, Result> HardwareDecoder::flush()
{
    releaseReturnedFrames();
    if (const Result rc = videoDecoderFlush(&m_decoder); R_FAILED(rc))
        return std::unexpected(rc);
    return {};
}

const void* HardwareDecoder::backing(std::uint32_t slot) const
{
    return m_decoder.images[slot].backing;
}

std::size_t HardwareDecoder::backingSize(std::uint32_t slot) const
{
    return static_cast<std::size_t>(m_decoder.images[slot].backing_size);
}

std::optional<OutputHandle> HardwareDecoder::output(std::uint32_t slot) const
{
    if (slot >= m_outputs.size())
        return std::nullopt;
    return m_outputs[slot]->handle();
}

void HardwareDecoder::returnFrame(const VideoFrame& frame)
{
    std::lock_guard lock(m_returned_mutex);
    m_returned.push_back(frame);
}

void HardwareDecoder::releaseReturnedFrames()
{
    std::vector<VideoFrame> returned;
    {
        std::lock_guard lock(m_returned_mutex);
        returned.swap(m_returned);
        m_returned.reserve(ImageCount);
    }
    std::ranges::for_each(returned, [this](VideoFrame& frame) { videoDecoderReleaseFrame(&m_decoder, &frame); });
    if (m_decoder.decoder.queue_count > ImageCount && m_reporter)
        m_reporter(std::format("released {} frames and the queue overflowed, {}", returned.size(), queueState()));
}

}
