#include "platform/video/drako3d/provider.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>

#include "platform/video/deko3d/dksh_shader_source.hpp"
#include "platform/video/drako3d/frame_decoder.hpp"

namespace akira::platform::drako3d
{

Drako3dVideoProvider::Drako3dVideoProvider()
    : m_deko3d(
          deko3d::Deko3dVideoProvider::Labels{
              .name = "drako3d", .decoderName = "Switch 2 hardware decoder", .decoderStat = "HW" },
          [] { return std::make_unique<ViddecFrameDecoder>(); },
          [] { return std::make_unique<deko3d::DkshShaderSource>(std::string(ShaderDirectory)); })
{
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
