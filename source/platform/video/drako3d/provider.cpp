#include "platform/video/drako3d/provider.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>

#include "platform/video/deko3d/dksh_shader_source.hpp"
#include "platform/video/drako3d/frame_decoder.hpp"
#include "platform/video/drako3d/output_memory.hpp"
#include "platform/video/software/hevc_decoder.hpp"
#include "platform/video/viddec/decoder.hpp"

namespace akira::platform::drako3d
{

Drako3dVideoProvider::Drako3dVideoProvider(play::StreamSettings& stream, const play::StreamState& streamState,
    settings::SettingsStore& store, const settings::DeveloperSettings& developer,
    const network::WireGuardManager& wireGuard)
    : m_stream(stream)
    , m_store(store)
    , m_deko3d("drako3d", { .stream = stream, .streamState = streamState, .wireGuard = wireGuard },
          deko3d::Deko3dVideoProvider::DecoderChoice{ .name = "Switch 2 hardware decoder", .stat = "HW",
              .make =
                  [&developer] {
                      auto decoder = std::make_unique<Nv12FrameDecoder<viddec::ViddecDecoder>>(developer);
                      decoder->decoder().setOutputMemory(std::make_shared<HostOutputMemory>());
                      return decoder;
                  } },
          [] { return std::make_unique<deko3d::DkshShaderSource>(std::string(ShaderDirectory)); },
          deko3d::Deko3dVideoProvider::DecoderChoice{ .name = "Switch 2 software HEVC decoder", .stat = "SW",
              .make = [] { return std::make_unique<Nv12FrameDecoder<software::HevcDecoder>>(); } })
{
}

bool Drako3dVideoProvider::addSettings(brls::Box& group) const
{
    software::HevcDecoder::addSetting(group, m_stream, m_store);
    return true;
}

bool Drako3dVideoProvider::shadersPresent()
{
    static constexpr auto required = std::to_array<std::string_view>({
        "romfs:/shaders/fill_vsh.dksh",
        "romfs:/shaders/fill_fsh.dksh",
        "romfs:/shaders/drako3d/video_vsh.dksh",
        "romfs:/shaders/drako3d/video_fsh.dksh",
    });
    std::error_code error;
    return std::ranges::all_of(required, [&](std::string_view path) { return std::filesystem::exists(path, error); });
}

}
