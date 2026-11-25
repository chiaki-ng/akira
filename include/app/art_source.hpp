#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace akira::app {

class ArtSource {
public:
    using Bytes = std::vector<std::byte>;
    using Delivery = std::function<void(std::optional<Bytes>)>;

    virtual ~ArtSource() = default;

    virtual void fetch(std::string url, Delivery done) = 0;
};

}
