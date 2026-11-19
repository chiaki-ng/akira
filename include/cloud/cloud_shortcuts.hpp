#pragma once

#include <vector>

#include "cloud/models.hpp"

namespace akira::cloud {

class CloudShortcuts {
public:
    enum class Toggle { Ignored, Added, Removed, NotBuilt, NeedsLibrary };

    explicit CloudShortcuts(std::vector<Game> games);

    auto toggle(const Game& game) -> Toggle;
    auto games() const -> const std::vector<Game>& { return pinned; }

private:
    std::vector<Game> pinned;
};

}
