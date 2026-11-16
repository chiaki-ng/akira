#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "platform/video/deko3d/shader_source.hpp"

namespace akira::platform::deko3d
{

class DkshShaderSource final : public ShaderSource
{
public:
    explicit DkshShaderSource(std::string directory);

    std::optional<Code> load(StaticShader shader) override;
    std::optional<Code> load(const VideoFragmentVariant& variant) override;
    DitherStrength ditherStrength() const override { return DitherStrength::Uniform; }

    static std::string fileName(const VideoFragmentVariant& variant);

private:
    std::optional<Code> read(std::string_view fileName) const;

    std::string m_directory;
};

}
