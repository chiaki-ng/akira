#include "play/session.hpp"
#include "play/stream_state.hpp"
#include "log/log.hpp"

#include "play/audio_manager.hpp"
#include "play/haptic_manager.hpp"
#include "play/input_manager.hpp"
#include "platform/video/renderer.hpp"
#include "app/crash_report.hpp"
#include "platform/factory.hpp"

#include <cstring>

namespace akira::play {

int Session::getHapticBase() const
{
    return m_haptic_manager ? m_haptic_manager->hapticBase : 400;
}

void Session::setHapticBase(int base)
{
    if (m_haptic_manager)
        m_haptic_manager->hapticBase = base;
}

void Session::setRumbleStrength(float strength)
{
    if (m_haptic_manager)
        m_haptic_manager->setRumbleStrength(strength);
}

void Session::setRumbleFreqs(float freqLow, float freqHigh)
{
    if (m_haptic_manager)
    {
        m_haptic_manager->setRumbleFreqLow(freqLow);
        m_haptic_manager->setRumbleFreqHigh(freqHigh);
    }
}

void Session::setEnvelopeDecay(float decay)
{
    if (m_haptic_manager)
        m_haptic_manager->setEnvelopeDecay(decay);
}

void Session::setEnvelopeAttack(float attack)
{
    if (m_haptic_manager)
        m_haptic_manager->setEnvelopeAttack(attack);
}

void Session::setRumbleCeiling(float ceiling)
{
    if (m_haptic_manager)
        m_haptic_manager->setRumbleCeiling(ceiling);
}

void Session::setRumbleSource(RumbleSource source)
{
    if (m_haptic_manager)
        m_haptic_manager->setRumbleSource(source);
}

void Session::setHapticIntensity(HapticIntensity intensity)
{
    if (m_haptic_manager)
        m_haptic_manager->setHapticIntensity(intensity);
}

Session::Session(akira::platform::PlatformFactory& platform, StreamState& streamState,
                 const ControllerSettings& controllers, const settings::DeveloperSettings& developer,
                 app::UiWindow& window, libchiaking::platform::ThreadingProvider& threading)
    : m_platform(platform), m_stream_state(streamState), m_window(window), m_threading(threading)
{
    m_audio_manager = std::make_unique<AudioManager>();
    m_input_manager = std::make_unique<InputManager>(*this, platform.pads(), controllers, window);
    m_haptic_manager = std::make_unique<HapticManager>(platform.createHaptics(), developer);
    m_haptic_manager->setInputManager(m_input_manager.get());
    m_pad_feedback = platform.createFeedback(*m_haptic_manager);
    m_video = platform.createVideo();
    m_video->ensureDecoder();
}

Session::~Session()
{
    FreeVideo();
}


void Session::describeStream(const libchiaking::media::StreamDescription& description)
{
    if (description.picture)
    {
        log::info("Stream granted {}x{} at {} fps, {} kbps, {}",
            description.picture->width, description.picture->height,
            description.picture->max_fps, description.picture->bitrate_kbps,
            description.picture->codec == libchiaking::media::VideoCodec::h264 ? "H.264" : "HEVC");
    }
    log::info("Stream audio {} channels at {} Hz", description.audio.channels, description.audio.rate);
    configureAudio(description.audio, false);
}

bool Session::configureAudio(const libchiaking::media::AudioFormat& format, bool raw_pcm)
{
    auto created = libchiaking::media::AudioDecoder::create(format, raw_pcm);
    if (!created)
    {
        log::error("Audio decoder rejected {} channels at {} Hz", format.channels, format.rate);
        m_audio_decoder.reset();
        return false;
    }
    m_audio_decoder.emplace(std::move(created).value());
    if (m_audio_manager)
    {
        m_audio_manager->cleanup();
        m_audio_manager->init(static_cast<unsigned int>(m_audio_decoder->output_channels()),
                              m_audio_decoder->sample_rate());
    }
    return true;
}

libchiaking::media::VideoResult Session::submitVideo(const libchiaking::media::VideoUnit& unit)
{
    using libchiaking::media::VideoVerdict;

    akira::platform::VideoDecoder* decoder = m_video->decoder();
    if (this->quit || !decoder)
        return {VideoVerdict::dropped, 0};

    if (unit.profile_changed() && unit.hevc() != decoder->isHEVC())
        log::warning("Stream sent {} video to a {} decoder",
            unit.hevc() ? "HEVC" : "H.264", decoder->isHEVC() ? "HEVC" : "H.264");

    if (!m_video_worker || !m_video_worker->submit(unit.profile_header(), unit.frame().bytes()))
        m_needs_recover.store(true, std::memory_order_relaxed);

    if (!m_takion_version_logged)
    {
        if (const auto play = attached())
        {
            if (const auto version = play->statistics().protocol_version)
            {
                log::info("Takion version negotiated: {}", *version);
                m_takion_version_logged = true;
            }
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - m_audio_counts_logged >= std::chrono::seconds(5))
    {
        if (m_audio_counts_logged != std::chrono::steady_clock::time_point{})
        {
            log::info("audio_units formats={} units={} without_decoder={} decode_failures={} samples_queued={}",
                m_audio_counts.formats.exchange(0, std::memory_order_relaxed),
                m_audio_counts.units.exchange(0, std::memory_order_relaxed),
                m_audio_counts.without_decoder.exchange(0, std::memory_order_relaxed),
                m_audio_counts.decode_failures.exchange(0, std::memory_order_relaxed),
                m_audio_counts.samples_queued.exchange(0, std::memory_order_relaxed));
        }
        m_audio_counts_logged = now;
    }

    if (m_needs_recover.exchange(false, std::memory_order_relaxed))
        return {VideoVerdict::recover, 0};

    return {m_presented.exchange(false, std::memory_order_relaxed) ? VideoVerdict::presented : VideoVerdict::accepted, 0};
}

libchiaking::media::AudioResult Session::submitAudio(const libchiaking::media::AudioUnit& unit)
{
    using libchiaking::media::AudioUnit;
    using libchiaking::media::AudioVerdict;

    if (unit.kind == AudioUnit::Kind::format)
    {
        m_audio_counts.formats.fetch_add(1, std::memory_order_relaxed);
        if (!configureAudio(unit.format, unit.raw_pcm))
            return {AudioVerdict::failed, 0};
        return {};
    }

    m_audio_counts.units.fetch_add(1, std::memory_order_relaxed);
    if (!m_audio_decoder)
    {
        m_audio_counts.without_decoder.fetch_add(1, std::memory_order_relaxed);
        return {AudioVerdict::failed, 0};
    }

    const auto pcm = m_audio_decoder->decode(unit.bytes);
    if (!pcm)
    {
        m_audio_counts.decode_failures.fetch_add(1, std::memory_order_relaxed);
        return {AudioVerdict::failed, 0};
    }

    if (m_audio_manager)
    {
        m_audio_manager->play(*pcm);
        m_audio_counts.samples_queued.fetch_add(pcm->size(), std::memory_order_relaxed);
    }

    if (m_haptic_manager && m_haptic_manager->isLocked())
        CleanUpHaptic();

    return {};
}

bool Session::InitVideo(int video_width, int video_height)
{
    log::info("load InitVideo");
    app::CrashReport::stage("stream video");
    std::lock_guard lock(m_media_mutex);
    this->quit = false;

    akira::platform::VideoDecoder* decoder = m_video->decoder();
    if (!decoder || !decoder->initVideo(video_width, video_height))
    {
        log::error("Failed to initialize video decoder");
        return false;
    }

    const akira::platform::VideoProvider::FrameHooks hooks{
        .arriving = [this](const akira::platform::FrameInfo& frame) { frameArriving(frame); },
        .presented = [this] {
            m_packets_since_frame.store(0, std::memory_order_relaxed);
            m_presented.store(true, std::memory_order_relaxed);
        },
    };
    if (!m_video->startRenderer(video_width, video_height, hooks))
    {
        log::error("Failed to initialize video renderer");
        return false;
    }

    m_needs_recover = false;
    m_presented = false;
    m_packets_since_frame = 0;
    m_video_worker = std::make_unique<VideoWorker>(
        [this](const AVBufferHandle& packet, std::size_t size) {
            akira::platform::VideoDecoder* decoder = m_video->decoder();
            if (!decoder->decode(packet, size) || decoder->lastDecodeCorrupt())
                m_needs_recover.store(true, std::memory_order_relaxed);
            checkForStall(*decoder);
        },
        m_threading);

    return true;
}

void Session::checkForStall(akira::platform::VideoDecoder& decoder)
{
    const std::uint32_t packets = m_packets_since_frame.fetch_add(1, std::memory_order_relaxed) + 1;
    if (packets < StallPackets)
        return;
    m_needs_recover.store(true, std::memory_order_relaxed);
    if (packets % StallPackets != 0)
        return;
    const std::uint32_t episode = packets / StallPackets;
    log::warning("Video stalled: {} packets without a frame, asking for a keyframe (stall {})", packets, episode);
    decoder.stalled(episode);
}

bool Session::FreeVideo()
{
    this->quit = true;
    app::CrashReport::stage("stream teardown");

    m_video_worker.reset();

    m_video->detachFrames();

    m_platform.stats().streamStopped();

    m_first_frame_received = false;
    m_stream_state.setActive(false);

    {
        std::lock_guard lock(m_media_mutex);
        m_video->release();
    }
    m_audio_decoder.reset();
    if (m_audio_manager)
        m_audio_manager->cleanup();

    resetStreamStats();

    return true;
}

void Session::SetRumble(uint8_t left, uint8_t right)
{
    if (m_haptic_manager)
    {
        m_haptic_manager->setRumble(left, right);
    }
}

void Session::ApplyPadFeedback(const libchiaking::controllers::ControllerFeedback& feedback)
{
    if (m_pad_feedback)
        m_pad_feedback->apply(feedback);
}

void Session::HapticCB(std::span<const std::byte> pcm)
{
    if (m_haptic_manager)
    {
        m_haptic_manager->processHapticAudio(pcm);
    }
}

void Session::CleanUpHaptic()
{
    if (m_haptic_manager)
    {
        m_haptic_manager->cleanup();
    }
}

void Session::updateActualResolution(int width, int height)
{
    if (width != m_requested_width || height != m_requested_height)
    {
        log::info("Stream resolution changed: requested {}x{}, actual {}x{} (server downgraded)",
            m_requested_width, m_requested_height, width, height);
    }

    if (akira::platform::VideoDecoder* decoder = m_video->decoder())
        decoder->updateResolution(width, height);

    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        renderer->updateResolution(width, height);
}

bool Session::InitAVCodec(bool hevc, bool ps5, int video_width, int video_height)
{
    std::lock_guard lock(m_media_mutex);
    return m_video->ensureDecoderFor(hevc).initCodec(hevc, ps5, video_width, video_height);
}

bool Session::InitController()
{
    return m_input_manager && m_input_manager->init();
}

bool Session::FreeController()
{
    if (m_input_manager)
        m_input_manager->cleanup();
    return true;
}

void Session::UpdateControllerState(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id)
{
    if (m_input_manager)
    {
        m_input_manager->update(input, finger_id_touch_id);
    }
}

void Session::UpdateSecondaryPads(std::span<libchiaking::controllers::ControllerInput> states, uint8_t& count)
{
    if (m_input_manager)
    {
        m_platform.pads().readSecondaryPads(*m_input_manager, states, count);
    }
}

bool Session::MainLoop()
{
    akira::platform::VideoRenderer* renderer = m_video->renderer();
    if (renderer && renderer->isInitialized() && m_video->decoder())
    {
        renderer->setStatsOverlayMode(m_stats_overlay_mode);
        if (m_stats_overlay_mode != StatsOverlayMode::Off)
        {
            renderer->setStreamStats(getStreamStats());
        }
        renderer->updateOverlayTexture();
    }

    return !this->quit;
}

void Session::frameArriving(const akira::platform::FrameInfo& frame)
{
    if (!m_first_frame_received)
    {
        m_first_frame_received = true;
        m_window.suspendRendering();
        updateActualResolution(static_cast<int>(frame.width), static_cast<int>(frame.height));
        m_stream_state.setActive(true);
        log::info("First video frame received!");
        m_platform.stats().streamStarted(*this);
    }

    if (akira::platform::VideoDecoder* decoder = m_video->decoder())
    {
        auto emit = decoder->lastEmitTime();
        if (emit != std::chrono::steady_clock::time_point{})
        {
            auto gap = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - emit).count();
            if (gap >= 0 && gap < 500000)
            {
                double ms = (double)gap / 1000.0;
                float prev = m_present_ms.load(std::memory_order_relaxed);
                m_present_ms.store(prev > 0.0f ? (float)(prev * 0.9 + ms * 0.1) : (float)ms,
                                   std::memory_order_relaxed);
            }
        }
    }
}

StreamStats Session::getStreamStats()
{
    std::lock_guard lock(m_media_mutex);
    StreamStats stats;

    stats.requested_width = m_requested_width;
    stats.requested_height = m_requested_height;
    stats.requested_fps = m_requested_fps;
    stats.requested_bitrate = m_requested_bitrate;
    stats.requested_hevc = m_requested_hevc;
    m_platform.pads().fillStreamStats(stats);

    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        stats.fps = renderer->getRenderFPS();

    if (akira::platform::VideoDecoder* decoder = m_video->decoder())
    {
        stats.video_width = decoder->getVideoWidth();
        stats.video_height = decoder->getVideoHeight();
        stats.is_hevc = decoder->isHEVC();
        stats.is_hardware_decoder = decoder->isHardwareAccelerated();

        auto ds = decoder->getStats();
        stats.decode_ms = ds.decode_ms;
        stats.source_fps = ds.source_fps;
        stats.jitter_ms = ds.jitter_ms;
        stats.decoder_drops = ds.decoder_drops;
        stats.present_ms = m_present_ms.load(std::memory_order_relaxed);
    }

    stats.renderer_name = m_video->name();

    if (const auto play = attached())
    {
        const libchiaking::play::Statistics observed = play->statistics();
        const auto& link = observed.link;
        if (link.reception.present)
        {
            stats.packets_received = link.reception.received;
            stats.packets_lost = link.reception.lost;
            stats.packet_loss_percent = static_cast<float>(link.reception.loss_ratio * 100.0);
        }
        if (link.video_metrics.present)
            stats.measured_bitrate_mbps = static_cast<float>(link.video_metrics.throughput_bps) / 1000000.0f;
        stats.rtt_valid = link.round_trip.present;
        stats.rtt_ms = static_cast<float>(link.round_trip.smoothed_us) / 1000.0f;
        stats.console_quality_valid = link.server.present;
        stats.console_loss = static_cast<uint64_t>(link.server.corrupt_frames);
        stats.upstream_loss = link.server.upstream_loss_percent;
        stats.target_bitrate_kbps = link.server.available_down_kbps;
        stats.network_frames_lost = static_cast<size_t>(observed.video.lost);
        stats.frames_recovered = static_cast<size_t>(observed.video.recovered);
        stats.rtt_variation_ms = static_cast<float>(link.round_trip.variation_us) / 1000.0f;
        stats.rtt_p99_ms = static_cast<float>(link.round_trip.p99_us) / 1000.0f;
        stats.rtt_recent_max_ms = static_cast<float>(link.round_trip.recent_max_us) / 1000.0f;
        stats.rtt_samples = link.round_trip.samples;
        stats.transfer_valid = link.video_metrics.present;
        stats.transfer_ms = static_cast<float>(link.video_metrics.transfer_time_us) / 1000.0f;
        stats.frames_received = observed.video.units;
        stats.keyframe_requests = observed.video.keyframe_requests;
        stats.keyframe_waits = link.dependency_rejections;
        stats.fec_repaired = link.fec_repaired_frames;
        stats.unassembled = link.assembly_rejections;
        stats.duplicates = link.video_duplicate;
        stats.stale_frames = link.video_stale;
        stats.decoder_rejections = observed.video.decoder_rejections;
        stats.rejected_packets = link.rejected_packets;
        stats.crypto_rejections = link.crypto_rejections;
        stats.reorder_skipped = link.reorder_skipped;
        stats.reorder_dropped = link.reorder_dropped;
        stats.internal_drops = link.dropped_posts + link.send_dropped;
        stats.session_errors = observed.loop_errors;
    }

    stats.visual_ms = stats.decode_ms + stats.present_ms;
    if (stats.rtt_valid)
    {
        stats.net_ms = stats.rtt_ms * 0.5f + stats.jitter_ms;
        stats.total_ms = stats.net_ms + stats.visual_ms;
        stats.latency_valid = true;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_stream_start_time);
    stats.stream_duration_seconds = duration.count();

    return stats;
}

void Session::setRequestedProfile(int width, int height, int fps, int bitrate, bool hevc)
{
    m_requested_width = width;
    m_requested_height = height;
    m_requested_fps = fps;
    m_requested_bitrate = bitrate;
    m_requested_hevc = hevc;
}

void Session::setVideoPaused(bool paused)
{
    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        renderer->setPaused(paused);
}

void Session::setConnectionContext(const std::string& context)
{
    m_connection_context = context;
    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        renderer->setConnectionContext(context);
}

UpscalingSummary Session::upscaling()
{
    std::lock_guard lock(m_media_mutex);
    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        return renderer->upscaling();
    return {};
}


void Session::triggerBorderFlash()
{
    if (akira::platform::VideoRenderer* renderer = m_video->renderer())
        renderer->triggerBorderFlash();
}

void Session::attach(std::weak_ptr<libchiaking::play::Session> play)
{
    std::lock_guard lock(m_play_mutex);
    m_play = std::move(play);
}

void Session::detach()
{
    std::lock_guard lock(m_play_mutex);
    m_play.reset();
}

std::shared_ptr<libchiaking::play::Session> Session::attached() const
{
    std::lock_guard lock(m_play_mutex);
    return m_play.lock();
}

void Session::startStreamTimer()
{
    m_stream_start_time = std::chrono::steady_clock::now();
}

void Session::resetStreamStats()
{
    detach();
}

}
