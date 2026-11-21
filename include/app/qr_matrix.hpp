#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace akira::app {

class QrMatrix {
public:
    static constexpr int quietModules = 2;

    static auto encode(std::string_view text) -> std::optional<QrMatrix>;

    auto size() const -> int { return side; }
    auto dark(int column, int row) const -> bool;
    auto modules() const -> const std::vector<bool>& { return cells; }

    friend bool operator==(const QrMatrix&, const QrMatrix&) = default;

private:
    QrMatrix(int side, std::vector<bool> modules);

    int side = 0;
    std::vector<bool> cells;
};

}
