#pragma once

#include <optional>
#include <string>

#include "cloud/models.hpp"

namespace akira::cloud {

struct TitleRefresh {
    enum class Outcome {
        Added,
        NotInLibrary,
        Failed
    };

    Outcome outcome = Outcome::Failed;
    std::optional<Game> game;
    std::string message;
};

}
