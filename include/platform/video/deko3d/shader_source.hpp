#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace akira::platform::deko3d
{

enum class StaticShader
{
    VideoVertex,
    OverlayVertex,
    OverlayFragment,
    BorderFragment,
};

struct VideoFragmentVariant
{
    bool dithering = false;
    bool fsrEasu = false;
    bool fsrRcas = false;
    bool fsrPass = false;

    bool operator==(const VideoFragmentVariant&) const = default;
};

enum class DitherStrength
{
    Baked,
    Uniform,
};

class ShaderSource
{
public:
    using Code = std::vector<std::uint8_t>;

    virtual ~ShaderSource() = default;

    virtual std::optional<Code> load(StaticShader shader) = 0;
    virtual std::optional<Code> load(const VideoFragmentVariant& variant) = 0;
    virtual DitherStrength ditherStrength() const = 0;

protected:
    static std::string_view stem(StaticShader shader);
    static bool isVertex(StaticShader shader);
    static std::optional<std::string> readFile(const std::string& path);
};

}
