#pragma once

#include <cstdint>
#include <vector>

namespace akira::play {

class PausedFrame {
public:
    PausedFrame() = default;
    PausedFrame(std::vector<std::uint8_t> pixels, int width, int height);

    bool ready() const { return !rgba.empty() && frameWidth > 0 && frameHeight > 0; }
    const std::uint8_t* data() const { return rgba.data(); }
    const std::vector<std::uint8_t>& pixels() const { return rgba; }
    int width() const { return frameWidth; }
    int height() const { return frameHeight; }
    void dropPixels();

private:
    static void blur(std::vector<std::uint8_t>& pixels, int width, int height);
    static void boxBlur(std::vector<std::uint8_t>& pixels, int width, int height, int radius);

    std::vector<std::uint8_t> rgba;
    int frameWidth = 0;
    int frameHeight = 0;
};

}
