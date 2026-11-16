#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "platform/video/deko3d/shader_source.hpp"

namespace akira::platform::deko3d
{

class UamShaderSource final : public ShaderSource
{
public:
    explicit UamShaderSource(std::string directory);
    ~UamShaderSource() override;
    UamShaderSource(const UamShaderSource&) = delete;
    UamShaderSource& operator=(const UamShaderSource&) = delete;

    std::optional<Code> load(StaticShader shader) override;
    std::optional<Code> load(const VideoFragmentVariant& variant) override;
    DitherStrength ditherStrength() const override { return DitherStrength::Baked; }

private:
    std::optional<Code> compile(const std::string& path, bool vertex, const std::string& defines) const;
    static std::string injectDefines(std::string source, const std::string& defines);
    static std::string fragmentDefines(const VideoFragmentVariant& variant);

    std::string m_directory;
};

}
