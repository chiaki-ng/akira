#ifndef AKIRA_IO_STREAM_STATS_HPP
#define AKIRA_IO_STREAM_STATS_HPP

#include <cstdint>
#include <cstddef>

enum class StatsOverlayMode
{
    Off = 0,
    Compact = 1,
    Full = 2
};

struct StreamStats
{
    // Requested profile (what user configured)
    int requested_width = 0;
    int requested_height = 0;
    int requested_fps = 0;
    int requested_bitrate = 0;
    bool requested_hevc = false;

    // Rendered info (what's actually being decoded)
    bool is_hardware_decoder = false;
    bool is_hevc = false;  // true = HEVC (PS5), false = H.264 (PS4)

    // Renderer info
    const char* renderer_name = "Unknown";

    // Frame stats
    float fps = 0.0f;

    int video_width = 0;
    int video_height = 0;

    float packet_loss_percent = 0.0f;
    uint64_t packets_received = 0;
    uint64_t packets_lost = 0;

    float measured_bitrate_mbps = 0.0f;

    size_t network_frames_lost = 0;
    size_t frames_recovered = 0;

    uint64_t stream_duration_seconds = 0;

    float rtt_ms = 0.0f;
    bool rtt_valid = false;
    uint64_t console_loss = 0;
    float upstream_loss = 0.0f;
    uint32_t target_bitrate_kbps = 0;
    bool console_quality_valid = false;

    float decode_ms = 0.0f;
    float present_ms = 0.0f;
    float jitter_ms = 0.0f;
    float source_fps = 0.0f;
    uint64_t decoder_drops = 0;

    float net_ms = 0.0f;
    float visual_ms = 0.0f;
    float total_ms = 0.0f;
    bool latency_valid = false;

    bool analog_triggers_active = false;
    uint8_t analog_l2 = 0;
    uint8_t analog_r2 = 0;
};

#endif // AKIRA_IO_STREAM_STATS_HPP
