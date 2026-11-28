#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "platform/video/deko3d/shader_source.hpp"

namespace akira::play {
class StreamSettings;
}

namespace akira::platform::uam
{

class UamShaderSource final : public deko3d::ShaderSource
{
public:
    UamShaderSource(std::string directory, const play::StreamSettings& stream);
    ~UamShaderSource() override;
    UamShaderSource(const UamShaderSource&) = delete;
    UamShaderSource& operator=(const UamShaderSource&) = delete;

    std::optional<Code> load(deko3d::StaticShader shader) override;
    std::optional<Code> load(const deko3d::VideoFragmentVariant& variant) override;
    deko3d::DitherStrength ditherStrength() const override { return deko3d::DitherStrength::Baked; }

private:
    static constexpr int kMinIssueDelay = 6;

    std::optional<Code> compile(const std::string& path, bool vertex, const std::string& defines) const;
    static std::string injectDefines(std::string source, const std::string& defines);
    std::string fragmentDefines(const deko3d::VideoFragmentVariant& variant) const;

    std::string m_directory;
    const play::StreamSettings& m_stream;
};

}
