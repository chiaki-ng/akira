#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "platform/video/deko3d/frame_decoder.hpp"
#include "platform/video/deko3d/shader_source.hpp"
#include "platform/video/provider.hpp"

namespace akira::platform::deko3d
{

class Deko3dRenderer;

class Deko3dVideoProvider final : public VideoProvider
{
public:
    struct Labels
    {
        const char* name;
        std::string_view decoderName;
        std::string_view decoderStat;
    };

    using DecoderFactory = std::function<std::unique_ptr<FrameDecoder>()>;
    using ShaderFactory = std::function<std::unique_ptr<ShaderSource>()>;

    Deko3dVideoProvider(Labels labels, DecoderFactory makeDecoder, ShaderFactory makeShaders);
    ~Deko3dVideoProvider() override;
    Deko3dVideoProvider(const Deko3dVideoProvider&) = delete;
    Deko3dVideoProvider& operator=(const Deko3dVideoProvider&) = delete;

    const char* name() const override { return m_labels.name; }
    std::string_view decoderName() const override { return m_labels.decoderName; }
    std::string_view decoderStat() const override { return m_labels.decoderStat; }
    bool addSettings(brls::Box&) const override { return false; }

    VideoDecoder* decoder() override;
    VideoDecoder& ensureDecoder() override;
    VideoRenderer* renderer() override;

    bool startRenderer(int width, int height, FrameHooks hooks) override;
    void detachFrames() override;
    void release() override;

private:
    Labels m_labels;
    DecoderFactory m_make_decoder;
    ShaderFactory m_make_shaders;
    std::unique_ptr<FrameDecoder> m_decoder;
    std::unique_ptr<Deko3dRenderer> m_renderer;
};

}
