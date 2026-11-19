#pragma once

#include <toml++/toml.hpp>

namespace akira::settings {

class Section {
public:
    virtual ~Section() = default;

    virtual void write(toml::table& config) const = 0;

protected:
    Section() = default;
    Section(const Section&) = default;
    Section& operator=(const Section&) = default;
};

}
