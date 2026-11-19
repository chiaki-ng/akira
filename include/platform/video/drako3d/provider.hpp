#pragma once

#include <string_view>
#include <utility>

#include "platform/video/deko3d/provider.hpp"

namespace akira::settings
{

class DeveloperSettings;

}

namespace akira::platform::drako3d
{

class Drako3dVideoProvider final : public VideoProvider
{
public:
    Drako3dVideoProvider(play::StreamSettings& stream, const play::StreamState& streamState,
        const settings::DeveloperSettings& developer,
        const network::WireGuardManager& wireGuard);
    Drako3dVideoProvider(const Drako3dVideoProvider&) = delete;
    Drako3dVideoProvider& operator=(const Drako3dVideoProvider&) = delete;

    static bool shadersPresent();

    const char* name() const override { return m_deko3d.name(); }
    std::string_view decoderName() const override { return m_deko3d.decoderName(); }
    std::string_view decoderStat() const override { return m_deko3d.decoderStat(); }
    VideoOptions options() const override { return { .softwareHevc = true }; }

    VideoDecoder* decoder() override { return m_deko3d.decoder(); }
    VideoDecoder& ensureDecoder() override { return m_deko3d.ensureDecoder(); }
    VideoDecoder& ensureDecoderFor(bool hevc) override { return m_deko3d.ensureDecoderFor(hevc); }
    VideoRenderer* renderer() override { return m_deko3d.renderer(); }

    bool startRenderer(int width, int height, FrameHooks hooks) override
    {
        return m_deko3d.startRenderer(width, height, std::move(hooks));
    }
    void detachFrames() override { m_deko3d.detachFrames(); }
    void release() override { m_deko3d.release(); }

private:
    static constexpr std::string_view ShaderDirectory = "romfs:/shaders/drako3d";

    deko3d::Deko3dVideoProvider m_deko3d;
};

}
