#pragma once

#include <expected>
#include <functional>
#include <string>

#include "app/screenshot.hpp"

namespace akira::app::arisa {

class StreamCapture {
public:
    using Delivery = std::move_only_function<void(std::expected<Screenshot, std::string>)>;

    virtual ~StreamCapture() = default;

    [[nodiscard]] virtual auto captureStream(Delivery& delivery) -> bool = 0;
};

}
