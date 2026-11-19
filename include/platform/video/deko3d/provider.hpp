#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "platform/video/deko3d/frame_decoder.hpp"
#include "platform/video/deko3d/gpu_host.hpp"
#include "platform/video/deko3d/shader_source.hpp"
#include "platform/video/overlay_palette.hpp"
#include "platform/video/provider.hpp"

namespace akira::network {
class WireGuardManager;
}

namespace akira::play {
class StreamSettings;
class StreamState;
}

namespace akira::platform::deko3d
{

class Deko3dRenderer;

class Deko3dVideoProvider final : public VideoProvider
{
public:
    using DecoderFactory = std::function<std::unique_ptr<FrameDecoder>()>;
    using ShaderFactory = std::function<std::unique_ptr<ShaderSource>()>;

    struct DecoderChoice
    {
        std::string_view name;
        std::string_view stat;
        DecoderFactory make;
    };

    struct Presentation
    {
        GpuHost& gpu;
        play::StreamSettings& stream;
        const play::StreamState& streamState;
        const network::WireGuardManager& wireGuard;
        const video::OverlayPalette& palette;
    };

    Deko3dVideoProvider(const char* name, Presentation presentation, DecoderChoice decoder, ShaderFactory makeShaders,
        std::optional<DecoderChoice> hevcDecoder = std::nullopt);
    ~Deko3dVideoProvider() override;
    Deko3dVideoProvider(const Deko3dVideoProvider&) = delete;
    Deko3dVideoProvider& operator=(const Deko3dVideoProvider&) = delete;

    const char* name() const override { return m_name; }
    std::string_view decoderName() const override { return active().name; }
    std::string_view decoderStat() const override { return active().stat; }
    VideoOptions options() const override { return {}; }

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoDecoder& ensureDecoderFor(bool hevc) override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    const DecoderChoice& active() const { return m_hevc_active ? *m_hevc_decoder : m_primary_decoder; }

    const char* m_name;
    Presentation m_presentation;
    DecoderChoice m_primary_decoder;
    std::optional<DecoderChoice> m_hevc_decoder;
    ShaderFactory m_make_shaders;
    bool m_hevc_active = false;
    std::unique_ptr<FrameDecoder> m_decoder;
    std::unique_ptr<Deko3dRenderer> m_renderer;
};

}
