#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace akira::app {

class BuildInfo {
public:
    struct Section {
        std::string title;
        std::vector<std::string> lines;

        friend bool operator==(const Section&, const Section&) = default;
    };

    static constexpr std::string_view file = "romfs:/build_info.txt";

    static auto read(const std::filesystem::path& path) -> std::optional<BuildInfo>;
    static auto parse(std::string_view text) -> BuildInfo;

    auto sections() const -> const std::vector<Section>& { return listed; }

    friend bool operator==(const BuildInfo&, const BuildInfo&) = default;

private:
    static auto heading(std::string_view line) -> std::optional<std::string>;

    std::vector<Section> listed;
};

}
