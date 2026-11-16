#include "platform/video/viddec/decoder.hpp"
#include "platform/video/viddec/h264_sps.hpp"
#include "core/settings_manager.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <borealis.hpp>

namespace akira::platform::viddec
{

ViddecDecoder::ViddecDecoder()
{
}

ViddecDecoder::~ViddecDecoder()
{
    cleanup();
}

bool ViddecDecoder::initCodec(bool hevc, bool ps5, int width, int height)
{
    m_is_hevc = hevc;
    m_reference_limit = ps5 ? Ps5ReferenceLimit : Ps4ReferenceLimit;
    m_video_width = width;
    m_video_height = height;
    if (hevc)
    {
        brls::Logger::error("ViddecDecoder: the Switch 2 hardware decoder has no HEVC support; request H.264");
        return false;
    }
    brls::Logger::info("ViddecDecoder: H.264 on the Switch 2 hardware decoder, up to {}x{}, {} references for the {}",
        width, height, m_reference_limit, ps5 ? "PS5" : "PS4");
    return true;
}

bool ViddecDecoder::initVideo(int video_width, int video_height)
{
    brls::Logger::info("ViddecDecoder: initVideo {}x{}", video_width, video_height);
    m_video_width = video_width;
    m_video_height = video_height;
    m_waiting_for_idr = true;
    return !m_is_hevc;
}

void ViddecDecoder::noteDecodeSample(std::chrono::steady_clock::duration d)
{
    m_decode_us_accum += (double)std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    m_decode_sample_count++;
}

void ViddecDecoder::notePacketArrival(std::chrono::steady_clock::time_point now)
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

ViddecDecoder::DecoderStats ViddecDecoder::getStats() const
{
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void ViddecDecoder::publishStats(std::chrono::steady_clock::time_point now)
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

    const std::uint32_t heldPeak = m_hardware ? m_hardware->takeHeldPeak() : 0;
    const std::uint64_t reused = m_hardware ? m_hardware->reusedWhileHeld() : 0;
    if (SettingsManager::getInstance()->getChiakiLogVerbosity() != ChiakiLogVerbosity::Normal)
        brls::Logger::info("ViddecDecoder: last {} ms submitted {} failed {} emitted {} held peak {} slots reused while held {}",
        elapsed_ms, m_window_submitted, m_window_failed, m_frames_emitted, heldPeak, reused);
    m_window_submitted = 0;
    m_window_failed = 0;

    m_window_start = now;
    m_decode_us_accum = 0.0;
    m_decode_sample_count = 0;
    m_frames_emitted = 0;
    m_interval_us_accum = 0.0;
    m_interval_dev_accum = 0.0;
    m_interval_count = 0;
}

std::vector<ViddecDecoder::NalUnit> ViddecDecoder::splitNalUnits(std::span<const std::uint8_t> sample)
{
    std::vector<NalUnit> units;
    std::optional<std::size_t> start;
    std::size_t offset = 0;
    auto close = [&](std::size_t end) {
        if (!start || *start >= end)
            return;
        while (end > *start && sample[end - 1] == 0x00)
            end--;
        if (end > *start)
            units.push_back({static_cast<std::uint8_t>(sample[*start] & 0x1F), sample.subspan(*start, end - *start)});
    };
    while (offset + 3 <= sample.size())
    {
        if (sample[offset] == 0x00 && sample[offset + 1] == 0x00 && sample[offset + 2] == 0x01)
        {
            close(offset);
            offset += 3;
            start = offset;
            continue;
        }
        offset++;
    }
    close(sample.size());
    return units;
}

ViddecDecoder::Keyframe ViddecDecoder::scanNalUnits(std::span<const std::uint8_t> sample)
{
    Keyframe keyframe;
    auto remember = [&](std::vector<std::uint8_t>& stored, std::span<const std::uint8_t> bytes, const char* name) {
        if (std::ranges::equal(stored, bytes))
            return;
        if (stored.empty())
            brls::Logger::info("ViddecDecoder: received {}", name);
        else
            keyframe.parameters_changed = true;
        stored.assign(bytes.begin(), bytes.end());
    };
    for (const NalUnit& unit : splitNalUnits(sample))
    {
        if (unit.type == NalSps)
        {
            keyframe.has_sps = true;
            if (!std::ranges::equal(m_sps, unit.bytes))
            {
                remember(m_sps, unit.bytes, "SPS");
                rememberDecoderSps();
            }
        }
        else if (unit.type == NalPps)
            remember(m_pps, unit.bytes, "PPS");
        else if (unit.type == NalIdr)
            keyframe.idr = true;
        if (unit.type >= NalSlice && unit.type <= NalIdr)
        {
            keyframe.has_slices = true;
            break;
        }
    }
    return keyframe;
}

std::vector<std::uint8_t> ViddecDecoder::codecConfig() const
{
    std::vector<std::uint8_t> config;
    config.reserve(StartCode.size() * 2 + m_decoder_sps.size() + m_pps.size());
    std::ranges::copy(StartCode, std::back_inserter(config));
    std::ranges::copy(m_decoder_sps, std::back_inserter(config));
    std::ranges::copy(StartCode, std::back_inserter(config));
    std::ranges::copy(m_pps, std::back_inserter(config));
    return config;
}

void ViddecDecoder::rememberDecoderSps()
{
    std::string hex;
    for (const std::uint8_t byte : m_sps)
        std::format_to(std::back_inserter(hex), "{:02x}", byte);
    const auto sps = akira::platform::viddec::H264Sps::parse(m_sps);
    if (!sps)
    {
        brls::Logger::warning("ViddecDecoder: SPS {} does not parse, giving it to the decoder unchanged", hex);
        m_decoder_sps = m_sps;
        return;
    }
    m_decoder_sps = sps->forDecoder(m_reference_limit);
    const auto given = akira::platform::viddec::H264Sps::parse(m_decoder_sps);
    brls::Logger::info("ViddecDecoder: SPS {} level {} refs {} dpb {}; decoder gets refs {} and no reordering",
        hex, sps->levelIdc(), sps->maxNumRefFrames(),
        sps->maxDecFrameBuffering() ? std::to_string(*sps->maxDecFrameBuffering()) : std::string("unset"),
        given ? given->maxNumRefFrames() : 0);
}

std::vector<std::uint8_t> ViddecDecoder::decoderSample(std::span<const std::uint8_t> sample) const
{
    std::vector<std::uint8_t> rebuilt;
    rebuilt.reserve(sample.size() + m_decoder_sps.size());
    for (const NalUnit& unit : splitNalUnits(sample))
    {
        std::ranges::copy(StartCode, std::back_inserter(rebuilt));
        if (unit.type == NalSps)
            std::ranges::copy(m_decoder_sps, std::back_inserter(rebuilt));
        else
            std::ranges::copy(unit.bytes, std::back_inserter(rebuilt));
    }
    return rebuilt;
}

bool ViddecDecoder::ensureHardwareDecoder(const Keyframe& keyframe)
{
    if (m_hardware && !keyframe.parameters_changed)
        return true;
    if (!keyframe.idr || m_sps.empty() || m_pps.empty())
        return false;

    const auto width = static_cast<std::uint32_t>((std::max(m_video_width, 16) + 15) & ~15);
    const auto height = static_cast<std::uint32_t>((std::max(m_video_height, 16) + 15) & ~15);
    m_hardware.reset();
    const auto config = codecConfig();
    auto created = akira::platform::viddec::HardwareDecoder::create({ViddecCodec_H264, width, height, config, m_output_memory});
    if (!created)
    {
        brls::Logger::error("ViddecDecoder: videoDecoderCreate {}x{} failed: 0x{:x}", width, height, created.error());
        return false;
    }
    m_hardware = std::move(created).value();
    m_hardware->setReporter([](const std::string& line) { brls::Logger::warning("HardwareDecoder: {}", line); });
    brls::Logger::info("ViddecDecoder: hardware decoder ready for {}x{}", width, height);
    if (m_hardware->zeroCopy())
        brls::Logger::info("ViddecDecoder: the decoder writes into renderer-imported memory, frames are sampled in place");
    else if (const auto& failure = m_hardware->outputFailure())
        brls::Logger::warning("ViddecDecoder: zero copy unavailable at {} (0x{:x}), frames are copied", failure->step, failure->code);
    return true;
}

void ViddecDecoder::emitReadyFrames(std::chrono::steady_clock::time_point decode_entry, std::chrono::nanoseconds first_wait)
{
    auto wait = first_wait;
    while (auto frame = m_hardware->nextFrame(wait))
    {
        wait = std::chrono::nanoseconds(0);
        m_last_emit = std::chrono::steady_clock::now();
        noteDecodeSample(m_last_emit - decode_entry);
        m_frames_emitted++;
        if (m_frame_ready_callback)
            m_frame_ready_callback(std::move(*frame));
        else
            m_drops++;
    }
}

bool ViddecDecoder::decode(const AVBufferHandle& data, std::size_t size)
{
    if (m_is_hevc || !data)
        return false;

    const std::span<const std::uint8_t> sample(data.get()->data, size);
    auto decode_entry = std::chrono::steady_clock::now();
    notePacketArrival(decode_entry);
    publishStats(decode_entry);

    const Keyframe keyframe = scanNalUnits(sample);
    if (!keyframe.has_slices)
        return true;
    if (m_waiting_for_idr)
    {
        if (!keyframe.idr || m_sps.empty() || m_pps.empty())
        {
            m_drops++;
            return false;
        }
        brls::Logger::info("ViddecDecoder: got a complete keyframe, decoding");
        m_waiting_for_idr = false;
    }

    if (!ensureHardwareDecoder(keyframe))
    {
        m_waiting_for_idr = true;
        return false;
    }

    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(decode_entry.time_since_epoch()).count();
    const auto rebuilt = keyframe.has_sps ? decoderSample(sample) : std::vector<std::uint8_t>{};
    const std::span<const std::uint8_t> submission = keyframe.has_sps ? std::span<const std::uint8_t>(rebuilt) : sample;
    m_window_submitted++;
    if (auto submitted = m_hardware->submit(submission, timestamp, SubmitTimeout); !submitted)
    {
        m_window_failed++;
        brls::Logger::warning("ViddecDecoder: videoDecoderSubmit failed: 0x{:x}", submitted.error());
        m_drops++;
        emitReadyFrames(decode_entry, std::chrono::nanoseconds(0));
        return false;
    }

    emitReadyFrames(decode_entry, FirstFrameWait);
    return true;
}

void ViddecDecoder::flush()
{
    brls::Logger::warning("ViddecDecoder: flushing, waiting for IDR");
    if (m_hardware)
    {
        if (auto flushed = m_hardware->flush(); !flushed)
            brls::Logger::warning("ViddecDecoder: videoDecoderFlush failed: 0x{:x}", flushed.error());
    }
    m_waiting_for_idr = true;
}

void ViddecDecoder::cleanup()
{
    m_hardware.reset();
    m_sps.clear();
    m_decoder_sps.clear();
    m_pps.clear();
}

}

