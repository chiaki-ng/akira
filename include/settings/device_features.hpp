#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "play/renderer_kind.hpp"

namespace akira::settings {

struct DeviceFeatures {
    struct Renderer {
        std::string label;
        play::RendererKind kind = play::RendererKind::Drako3d;

        friend bool operator==(const Renderer&, const Renderer&) = default;
    };

    bool selfUpdate = false;
    bool statsExport = false;
    bool fsr4k = false;
    bool hevc = false;
    bool cloud = false;
    bool accounts = false;
    std::vector<Renderer> renderers;
    std::string videoName;
    bool softwareHevc = false;
    bool zeroCopy = false;
    std::filesystem::path configFile;
    std::filesystem::path buildInfoFile;
    std::filesystem::path logFolder;
    std::vector<std::uint32_t> takionPs4;
    std::vector<std::uint32_t> takionPs5;
    std::vector<std::uint32_t> takionPs5Cloud;
    std::vector<std::uint32_t> takionPsNow;

    friend bool operator==(const DeviceFeatures&, const DeviceFeatures&) = default;
};

}
