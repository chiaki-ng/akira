#pragma once

#include <string>
#include <vector>

namespace akira::network {

struct NetworkFacts {
    enum class Psn { Checking, Reachable, Unreachable, Skipped };

    std::string address;
    std::vector<std::string> systemDns;
    std::vector<std::string> akiraDns;
    Psn psn = Psn::Checking;

    friend bool operator==(const NetworkFacts&, const NetworkFacts&) = default;
};

}
