#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "platform/video/deko3d/shader_source.hpp"

namespace akira::platform::uam
{

class UamShaderSource final : public deko3d::ShaderSource
{
public:
    explicit UamShaderSource(std::string directory);
    ~UamShaderSource() override;
    UamShaderSource(const UamShaderSource&) = delete;
    UamShaderSource& operator=(const UamShaderSource&) = delete;

    std::optional<Code> load(deko3d::StaticShader shader) override;
    std::optional<Code> load(const deko3d::VideoFragmentVariant& variant) override;
    deko3d::DitherStrength ditherStrength() const override { return deko3d::DitherStrength::Baked; }

private:
    std::optional<Code> compile(const std::string& path, bool vertex, const std::string& defines) const;
    static std::string injectDefines(std::string source, const std::string& defines);
    static std::string fragmentDefines(const deko3d::VideoFragmentVariant& variant);

    std::string m_directory;
};

}
