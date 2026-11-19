#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "consoles/host_type.hpp"

namespace akira::consoles {

class Host;

class DebugConsoles {
public:
    struct Console {
        std::string nickname;
        bool resting = false;
        HostType type = HostType::Manual;
    };

    using Make = std::function<std::shared_ptr<Host>(const Console&)>;

    static constexpr int Count = 20;

    static std::vector<Console> list();

    explicit DebugConsoles(const Make& make);

    const std::vector<std::shared_ptr<Host>>& hosts() const { return owned; }

private:
    std::vector<std::shared_ptr<Host>> owned;
};

}
