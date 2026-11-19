#include "platform/video/uam/shader_source.hpp"
#include "log/log.hpp"

#include <cstdlib>
#include <format>
#include <utility>
#include <memory>

#include <uam.h>

#include "settings/settings_manager.hpp"

namespace akira::platform::uam
{

UamShaderSource::UamShaderSource(std::string directory)
    : m_directory(std::move(directory))
{
    uam_init();
}

UamShaderSource::~UamShaderSource()
{
    uam_deinit();
}

std::optional<deko3d::ShaderSource::Code> UamShaderSource::load(deko3d::StaticShader shader)
{
    return compile(std::format("{}/{}.glsl", m_directory, stem(shader)), isVertex(shader), {});
}

std::optional<deko3d::ShaderSource::Code> UamShaderSource::load(const deko3d::VideoFragmentVariant& variant)
{
    return compile(std::format("{}/video_fsh.glsl", m_directory), false, fragmentDefines(variant));
}

std::optional<deko3d::ShaderSource::Code> UamShaderSource::compile(const std::string& path, bool vertex,
    const std::string& defines) const
{
    auto source = readFile(path);
    if (!source)
        return std::nullopt;

    const std::string glsl = injectDefines(std::move(*source), defines);
    const char* kind = vertex ? "vertex" : "fragment";
    akira::log::info("UamShaderSource: compiling {} shader {} ({} bytes)", kind, path, glsl.size());

    std::uint8_t* raw = nullptr;
    std::uint32_t size = 0;
    if (!uam_compileDksh(vertex ? uam_pipeline_stage_vertex : uam_pipeline_stage_fragment,
            glsl.c_str(), 3, &raw, &size))
    {
        akira::log::error("UamShaderSource: failed to compile {} shader {}", kind, path);
        return std::nullopt;
    }
    std::unique_ptr<std::uint8_t, decltype(&std::free)> owned(raw, &std::free);

    akira::log::info("UamShaderSource: {} shader compiled ({} bytes dksh)", kind, size);
    return Code(owned.get(), owned.get() + size);
}

std::string UamShaderSource::injectDefines(std::string source, const std::string& defines)
{
    if (defines.empty())
        return source;

    const std::size_t version = source.find("#version");
    const std::size_t newline = source.find('\n', version == std::string::npos ? 0 : version);
    if (newline == std::string::npos)
        return source + "\n" + defines;

    source.insert(newline + 1, defines);
    return source;
}

std::string UamShaderSource::fragmentDefines(const deko3d::VideoFragmentVariant& variant)
{
    std::string defines;
    if (variant.fsrEasu)
        defines += "#define FSR_EASU\n";
    if (variant.fsrRcas)
        defines += "#define FSR_RCAS\n";
    if (variant.fsrPass)
        defines += "#define FSR_PASS\n";
    if (variant.dithering)
        defines += std::format("#define DITHER_NOISE\n#define DITHER_STRENGTH {:.1f}\n",
            settings::SettingsManager::getInstance()->getDitheringStrength());
    return defines;
}

}
