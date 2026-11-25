#pragma once

#include <cstdint>
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

protected:
    auto pattern(std::string_view key, std::int64_t count) const -> std::string override
    {
        if (count == 1)
            if (const auto single = entries.find(std::string(key) + "_one"); single != entries.end())
                return single->second;
        return text(key);
    }

private:
    std::map<std::string, std::string, std::less<>> entries;
};

}
