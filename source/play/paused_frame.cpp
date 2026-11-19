#include "play/paused_frame.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace akira::play {

PausedFrame::PausedFrame(std::vector<std::uint8_t> pixels, int width, int height)
    : rgba(std::move(pixels))
    , frameWidth(width)
    , frameHeight(height)
{
    blur(rgba, frameWidth, frameHeight);
}

void PausedFrame::dropPixels()
{
    rgba.clear();
    rgba.shrink_to_fit();
}

void PausedFrame::blur(std::vector<std::uint8_t>& pixels, int width, int height)
{
    // The menu frame is already captured at 640 px wide. Three small box
    // passes approximate a soft Gaussian blur while keeping this one-time
    // transition comfortably cheaper than a live framebuffer effect.
    constexpr int kPassRadius = 5;
    for (int pass = 0; pass < 3; pass++)
        boxBlur(pixels, width, height, kPassRadius);
}

void PausedFrame::boxBlur(std::vector<std::uint8_t>& pixels, int width, int height, int radius)
{
    if (radius <= 0 || width <= 1 || height <= 1 ||
        pixels.size() != static_cast<size_t>(width) * height * 4)
        return;

    std::vector<uint8_t> scratch(pixels.size());
    const int diameter = radius * 2 + 1;

    for (int y = 0; y < height; y++)
    {
        int sums[4] = {};
        for (int dx = -radius; dx <= radius; dx++)
        {
            const int sx = std::clamp(dx, 0, width - 1);
            const size_t src = (static_cast<size_t>(y) * width + sx) * 4;
            for (int c = 0; c < 4; c++)
                sums[c] += pixels[src + c];
        }

        for (int x = 0; x < width; x++)
        {
            const size_t dst = (static_cast<size_t>(y) * width + x) * 4;
            for (int c = 0; c < 4; c++)
                scratch[dst + c] = static_cast<uint8_t>(sums[c] / diameter);

            const int removeX = std::clamp(x - radius, 0, width - 1);
            const int addX = std::clamp(x + radius + 1, 0, width - 1);
            const size_t remove = (static_cast<size_t>(y) * width + removeX) * 4;
            const size_t add = (static_cast<size_t>(y) * width + addX) * 4;
            for (int c = 0; c < 4; c++)
                sums[c] += static_cast<int>(pixels[add + c]) - pixels[remove + c];
        }
    }

    for (int x = 0; x < width; x++)
    {
        int sums[4] = {};
        for (int dy = -radius; dy <= radius; dy++)
        {
            const int sy = std::clamp(dy, 0, height - 1);
            const size_t src = (static_cast<size_t>(sy) * width + x) * 4;
            for (int c = 0; c < 4; c++)
                sums[c] += scratch[src + c];
        }

        for (int y = 0; y < height; y++)
        {
            const size_t dst = (static_cast<size_t>(y) * width + x) * 4;
            for (int c = 0; c < 4; c++)
                pixels[dst + c] = static_cast<uint8_t>(sums[c] / diameter);

            const int removeY = std::clamp(y - radius, 0, height - 1);
            const int addY = std::clamp(y + radius + 1, 0, height - 1);
            const size_t remove = (static_cast<size_t>(removeY) * width + x) * 4;
            const size_t add = (static_cast<size_t>(addY) * width + x) * 4;
            for (int c = 0; c < 4; c++)
                sums[c] += static_cast<int>(scratch[add + c]) - scratch[remove + c];
        }
    }
}

}
