#include "platform/video/deko3d/shader_source.hpp"

#include <fstream>
#include <iterator>

#include <borealis.hpp>

namespace akira::platform::deko3d
{

std::string_view ShaderSource::stem(StaticShader shader)
{
    switch (shader)
    {
    case StaticShader::VideoVertex: return "video_vsh";
    case StaticShader::OverlayVertex: return "overlay_vsh";
    case StaticShader::OverlayFragment: return "overlay_fsh";
    case StaticShader::BorderFragment: return "border_fsh";
    }
    return {};
}

bool ShaderSource::isVertex(StaticShader shader)
{
    return shader == StaticShader::VideoVertex || shader == StaticShader::OverlayVertex;
}

std::optional<std::string> ShaderSource::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        brls::Logger::error("Failed to open shader file: {}", path);
        return std::nullopt;
    }
    std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (contents.empty())
    {
        brls::Logger::error("Shader file is empty: {}", path);
        return std::nullopt;
    }
    return contents;
}

}
