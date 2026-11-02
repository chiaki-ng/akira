#ifndef AKIRA_SESSION_HPP
#define AKIRA_SESSION_HPP

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>

#include <libchiaking/play.hpp>
#include <switch.h>
#include <chrono>

#include "core/exception.hpp"
#include "input/rumble_profile.hpp"
#include "stream/stream_stats.hpp"
#include "stream/video_worker.hpp"

class AudioManager;
class HapticManager;
class InputManager;
class VideoDecoder;
class IVideoRenderer;
class IpcStatsService;
typedef struct AVFrame AVFrame;

class Session
{
protected:
    Session();

private:
    std::atomic<bool> quit = false;

    std::unique_ptr<AudioManager> m_audio_manager;
    std::unique_ptr<HapticManager> m_haptic_manager;
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<VideoDecoder> m_video_decoder;
    std::unique_ptr<IVideoRenderer> m_video_renderer;
    std::unique_ptr<akira::stream::VideoWorker> m_video_worker;
    std::unique_ptr<IpcStatsService> m_ipc_service;
    std::optional<libchiaking::media::AudioDecoder> m_audio_decoder;

    StatsOverlayMode m_stats_overlay_mode = StatsOverlayMode::Off;
    std::atomic<float> m_present_ms{0.0f};

    std::atomic<bool> m_first_frame_received = false;
    std::atomic<bool> m_presented{false};
    std::atomic<bool> m_needs_recover{false};
    bool m_takion_version_logged = false;
    struct AudioCounts {
        std::atomic<std::uint64_t> units{0};
        std::atomic<std::uint64_t> formats{0};
        std::atomic<std::uint64_t> without_decoder{0};
        std::atomic<std::uint64_t> decode_failures{0};
        std::atomic<std::uint64_t> samples_queued{0};
    };
    AudioCounts m_audio_counts;
    std::chrono::steady_clock::time_point m_audio_counts_logged{};

    int m_requested_width = 0;
    int m_requested_height = 0;
    int m_requested_fps = 0;
    int m_requested_bitrate = 0;
    bool m_requested_hevc = false;

    std::mutex m_media_mutex;
    mutable std::mutex m_play_mutex;
    std::weak_ptr<libchiaking::play::Session> m_play;
    std::chrono::steady_clock::time_point m_stream_start_time;

    void presentDecodedFrame(AVFrame* frame);
    bool configureAudio(const libchiaking::media::AudioFormat& format, bool raw_pcm);
    std::shared_ptr<libchiaking::play::Session> attached() const;

public:
    Session(const Session&) = delete;
    void operator=(const Session&) = delete;
    static Session* GetInstance();

    int getHapticBase() const;
    void setHapticBase(int base);
    void setRumbleStrength(float strength);
    void setRumbleFreqs(float freqLow, float freqHigh);
    void setEnvelopeDecay(float decay);
    void setEnvelopeAttack(float attack);
    void setRumbleCeiling(float ceiling);
    void setRumbleSource(akira::input::RumbleSource source);

    ~Session();

    void SetMesaConfig();
    void describeStream(const libchiaking::media::StreamDescription& description);
    libchiaking::media::VideoResult submitVideo(const libchiaking::media::VideoUnit& unit);
    libchiaking::media::AudioResult submitAudio(const libchiaking::media::AudioUnit& unit);
    bool InitVideo(int video_width, int video_height);
    bool InitAVCodec(bool hevc, int video_width = 0, int video_height = 0);
    bool FreeVideo();
    bool InitController();
    bool FreeController();
    bool MainLoop();
    void UpdateControllerState(libchiaking::controllers::ControllerInput& input, std::map<uint32_t, int8_t>* finger_id_touch_id);

    void UpdateSecondaryPads(std::span<libchiaking::controllers::ControllerInput> states, uint8_t& count);
    void SetRumble(uint8_t left, uint8_t right);

    void SetTriggerEffects(const libchiaking::controllers::TriggerEffects& effects);

    void SetEffectIntensity(uint8_t vibration, uint8_t trigger);

    void SetLedColor(uint8_t red, uint8_t green, uint8_t blue);

private:
    uint8_t m_trigger_intensity = 1;

public:
    void HapticCB(std::span<const std::byte> pcm);
    void CleanUpHaptic();

    StreamStats getStreamStats();
    StatsOverlayMode getStatsOverlayMode() const { return m_stats_overlay_mode; }
    void setStatsOverlayMode(StatsOverlayMode mode) { m_stats_overlay_mode = mode; }
    void setVideoPaused(bool paused);
    void setConnectionContext(const std::string& context);
    void triggerBorderFlash();
    void setRequestedProfile(int width, int height, int fps, int bitrate, bool hevc);

    bool hasReceivedFirstFrame() const { return m_first_frame_received; }
    void updateActualResolution(int width, int height);

    InputManager* getInputManager() { return m_input_manager.get(); }
    IVideoRenderer* getVideoRenderer() { return m_video_renderer.get(); }

    void attach(std::weak_ptr<libchiaking::play::Session> play);
    void detach();
    void startStreamTimer();
    void resetStreamStats();
};

#endif // AKIRA_SESSION_HPP
