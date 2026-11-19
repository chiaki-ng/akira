#include "play/views/stream_cards.hpp"
#include "play/session.hpp"
#include "app/views/theme.hpp"

#include <borealis/core/i18n.hpp>
#include <format>

using namespace brls::literals;

namespace
{
    NVGcolor fade(NVGcolor c, float a)
    {
        c.a *= a;
        return c;
    }

    struct Readout
    {
        std::string key;
        std::string value;
        std::string unit;
        NVGcolor tone;
    };

    void drawReadouts(NVGcontext* vg, const CardTheme& t, float x, float y,
                      const std::vector<Readout>& items)
    {
        float cursor = x;

        for (const auto& it : items)
        {
            nvgFontFaceId(vg, t.font_ui);
            nvgFontSize(vg, 10.5f * t.s);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(vg, fade(akira::app::views::active().textMuted, t.alpha));
            nvgText(vg, cursor, y, it.key.c_str(), nullptr);
            float keyW = nvgTextBounds(vg, 0, 0, it.key.c_str(), nullptr, nullptr);

            float vy = y + 17.0f * t.s;
            nvgFontFaceId(vg, t.font_mono);
            nvgFontSize(vg, 21.0f * t.s);
            nvgFillColor(vg, fade(it.tone, t.alpha));
            nvgText(vg, cursor, vy, it.value.c_str(), nullptr);
            float valW = nvgTextBounds(vg, 0, 0, it.value.c_str(), nullptr, nullptr);

            float unitW = 0.0f;
            if (!it.unit.empty())
            {
                nvgFontFaceId(vg, t.font_ui);
                nvgFontSize(vg, 11.5f * t.s);
                nvgFillColor(vg, fade(akira::app::views::active().textDim, t.alpha));
                nvgText(vg, cursor + valW + 3.0f * t.s, vy + 8.0f * t.s, it.unit.c_str(), nullptr);
                unitW = nvgTextBounds(vg, 0, 0, it.unit.c_str(), nullptr, nullptr) + 3.0f * t.s;
            }

            float block = keyW > valW + unitW ? keyW : valW + unitW;
            cursor += block + 34.0f * t.s;
        }
    }
}

StatsCard::StatsCard(StatsOverlayMode mode, std::function<void(StatsOverlayMode)> apply)
    : mode(mode)
    , apply(std::move(apply))
{
}

std::string StatsCard::label() const
{
    return "akira/stream_menu/card_stats"_i18n;
}

CardState StatsCard::state() const
{
    return mode == StatsOverlayMode::Off ? CardState::Idle : CardState::Active;
}

std::string StatsCard::optionLabel(int i) const
{
    switch (i)
    {
        case 0:  return "akira/stream_menu/opt_off"_i18n;
        case 1:  return "akira/stream_menu/opt_compact"_i18n;
        default: return "akira/stream_menu/opt_full"_i18n;
    }
}

void StatsCard::chooseOption(int i)
{
    StatsOverlayMode next = StatsOverlayMode::Off;
    if (i == 1)
        next = StatsOverlayMode::Compact;
    else if (i == 2)
        next = StatsOverlayMode::Full;

    mode = next;
    if (apply)
        apply(next);
}

std::string StatsCard::panelTitle() const
{
    return "akira/stream_menu/panel_stats_title"_i18n;
}

float StatsCard::panelHeight(const CardTheme& t) const
{
    return 232.0f * t.s;
}

void StatsCard::drawPanel(NVGcontext* vg, const CardTheme& t, float x, float y, float w, float h)
{
    (void)w; (void)h;

    Session* session = Session::GetInstance();
    if (!session)
        return;

    StreamStats st = session->getStreamStats();

    const akira::app::views::Palette& p = akira::app::views::active();
    NVGcolor kVal = p.text;
    NVGcolor kGood = p.success;
    NVGcolor kWarn = p.warning;
    NVGcolor kBad = p.danger;

    NVGcolor fpsTone = kGood;
    if (st.requested_fps > 0)
    {
        float r = st.fps / (float)st.requested_fps;
        fpsTone = r >= 0.92f ? kGood : (r >= 0.75f ? kWarn : kBad);
    }

    NVGcolor lossTone = st.packet_loss_percent < 0.5f ? kGood
                      : (st.packet_loss_percent < 2.0f ? kWarn : kBad);

    NVGcolor rttTone = kVal;
    std::string rtt = "--";
    if (st.rtt_valid)
    {
        rtt = std::format("{:.0f}", st.rtt_ms);
        rttTone = st.rtt_ms < 30.0f ? kGood : (st.rtt_ms < 70.0f ? kWarn : kBad);
    }

    NVGcolor latTone = kVal;
    std::string total = "--";
    std::string net = "--";
    if (st.latency_valid)
    {
        total = std::format("{:.0f}", st.total_ms);
        net = std::format("{:.0f}", st.net_ms);
        latTone = st.total_ms < 50.0f ? kGood : (st.total_ms < 100.0f ? kWarn : kBad);
    }

    std::vector<Readout> link = {
        { "akira/stream_menu/stat_fps"_i18n,    std::format("{:.1f}", st.fps),                   "",     fpsTone },
        { "akira/stream_menu/stat_rate"_i18n,   std::format("{:.1f}", st.measured_bitrate_mbps), "Mbps", kVal },
        { "akira/stream_menu/stat_rtt"_i18n,    rtt,                                             "ms",   rttTone },
        { "akira/stream_menu/stat_loss"_i18n,   std::format("{:.1f}", st.packet_loss_percent),   "%",    lossTone },
        { "akira/stream_menu/stat_lost"_i18n,   std::format("{}", st.network_frames_lost),       "",     kVal },
        { "akira/stream_menu/stat_decode"_i18n, st.is_hardware_decoder ? std::string(Session::GetInstance()->video().decoderStat()) : "SW", "", kVal },
    };

    std::vector<Readout> latency = {
        { "akira/stream_menu/stat_total"_i18n,  total,                                        "ms", latTone },
        { "akira/stream_menu/stat_net"_i18n,    net,                                          "ms", kVal },
        { "akira/stream_menu/stat_visual"_i18n, std::format("{:.1f}", st.visual_ms),          "ms", kVal },
        { "akira/stream_menu/stat_jitter"_i18n, std::format("{:.1f}", st.jitter_ms),          "ms", kVal },
        { "akira/stream_menu/stat_source"_i18n, std::format("{:.1f}", st.source_fps),         "",   kVal },
    };

    drawReadouts(vg, t, x, y, link);
    drawReadouts(vg, t, x, y + 54.0f * t.s, latency);
}

PowerCard::PowerCard(bool sleepAvailable, std::function<void(bool)> disconnect)
    : sleepAvailable(sleepAvailable)
    , disconnect(std::move(disconnect))
{
}

std::string PowerCard::label() const
{
    return "akira/stream_menu/card_power"_i18n;
}

std::string PowerCard::panelTitle() const
{
    return "akira/stream_menu/panel_power_title"_i18n;
}

float PowerCard::panelHeight(const CardTheme& t) const
{
    return 132.0f * t.s;
}

std::string PowerCard::optionLabel(int i) const
{
    if (i == 0)
        return "akira/stream_menu/opt_disconnect"_i18n;
    return "akira/stream_menu/opt_disconnect_sleep"_i18n;
}

void PowerCard::chooseOption(int i)
{
    if (disconnect)
        disconnect(i == 1);
}

std::string PadCard::label() const
{
    return "akira/stream_menu/card_pad"_i18n;
}

std::string MotionCard::label() const
{
    return "akira/stream_menu/card_motion"_i18n;
}

std::string PlayersCard::label() const
{
    return "akira/stream_menu/card_players"_i18n;
}
