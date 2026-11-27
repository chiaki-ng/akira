#include "platform/factories/nx/factory.hpp"

#include "platform/feedback/extended/provider.hpp"
#include "platform/haptics/extended/provider.hpp"
#include "platform/video/deko3d/provider.hpp"
#include "platform/video/gpu_access.hpp"
#include "platform/video/nvtegra/decoder.hpp"
#include "platform/video/uam/shader_source.hpp"
#include "platform/pad_settings/extended/provider.hpp"

namespace akira::platform
{

std::unique_ptr<PlatformFactory> PlatformFactory::create(PlatformServices services)
{
    return std::make_unique<NxPlatformFactory>(services);
}

PlatformCapability PlatformFactory::fixedCapabilities() noexcept
{
    return NxPlatformFactory::Capabilities;
}

NxPlatformFactory::NxPlatformFactory(PlatformServices services)
    : m_services(services)
    , m_updates(services.app, "chiaki-ng/akira")
    , m_stats(services.developer, services.wireGuard)
    , m_pads(services.controllers, services.threading, services.rumble)
{
}

PlatformInfo NxPlatformFactory::info() const noexcept
{
    return {"nx", Capabilities};
}

void NxPlatformFactory::prepareVideo()
{
}

std::vector<RendererChoice> NxPlatformFactory::rendererChoices() const
{
    return {};
}

std::unique_ptr<VideoProvider> NxPlatformFactory::createVideo()
{
    return std::make_unique<deko3d::Deko3dVideoProvider>("Deko3d",
        deko3d::Deko3dVideoProvider::Presentation{ .gpu = m_services.gpu.deko3d().value().get(),
            .stream = m_services.stream, .streamState = m_services.streamState, .wireGuard = m_services.wireGuard,
            .palette = m_services.overlayPalette },
        deko3d::Deko3dVideoProvider::DecoderChoice{ .name = "FFmpeg nvtegra", .stat = "NVTEGRA",
            .make = [] { return std::make_unique<nvtegra::NvtegraDecoder>(); } },
        [&stream = m_services.stream] { return std::make_unique<uam::UamShaderSource>("romfs:/shaders", stream); });
}

std::unique_ptr<HapticsProvider> NxPlatformFactory::createHaptics()
{
    return std::make_unique<extended::ExtendedHapticsProvider>(m_pads, m_services.controllers, m_services.developer);
}

std::unique_ptr<FeedbackProvider> NxPlatformFactory::createFeedback(play::HapticManager& haptics)
{
    return std::make_unique<extended::ExtendedFeedbackProvider>(m_pads, haptics);
}

std::unique_ptr<PadSettingsProvider> NxPlatformFactory::createPadSettings(const platform::PadDescription& pad)
{
    return std::make_unique<extended::ExtendedPadSettingsProvider>(pad, m_services.controllers);
}

}
