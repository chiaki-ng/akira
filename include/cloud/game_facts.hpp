#pragma once

#include <string>
#include <vector>

#include "cloud/models.hpp"

namespace akira::cloud {

class GameFacts {
public:
    enum class Fact { Publisher, Released, Genres };

    struct Entry {
        Fact fact;
        std::string value;
    };

    explicit GameFacts(const GameDetail& detail);

    auto entries() const -> const std::vector<Entry>& { return shown; }

private:
    static auto releaseDate(int64_t releasedMs) -> std::string;

    std::vector<Entry> shown;
};

}
