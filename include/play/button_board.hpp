#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace akira::play {

class ControllerSettings;

class ButtonBoard {
public:
    enum class Side { Leading, Trailing, Touchpad };

    struct Target {
        std::uint32_t key = 0;
        std::string_view name;
        std::string_view image;
        Side side = Side::Leading;
    };

    explicit ButtonBoard(ControllerSettings& settings);

    static auto targets() -> const std::vector<Target>&;
    static auto label(std::uint64_t button) -> std::string_view;
    static auto image(std::uint64_t button) -> std::string_view;

    auto mapping(std::uint32_t key) const -> std::vector<std::uint64_t>;
    auto isDefault(std::uint32_t key) const -> bool;
    auto conflicts(std::uint32_t key) const -> bool;

    void assign(std::uint32_t key, std::vector<std::uint64_t> combo);
    void reset(std::uint32_t key);
    void resetAll();

private:
    static auto sorted(std::vector<std::uint64_t> buttons) -> std::vector<std::uint64_t>;

    ControllerSettings* settings;
};

}
