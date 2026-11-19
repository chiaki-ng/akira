#pragma once

#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "app/strings.hpp"

namespace tests {

class FakeStrings : public akira::app::Strings {
public:
    explicit FakeStrings(std::map<std::string, std::string, std::less<>> entries) : entries(std::move(entries)) {}

    auto text(std::string_view key) const -> std::string override
    {
        const auto found = entries.find(key);
        return found == entries.end() ? std::string(key) : found->second;
    }

private:
    std::map<std::string, std::string, std::less<>> entries;
};

}
