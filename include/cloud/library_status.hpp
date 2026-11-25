#pragma once

#include <optional>
#include <vector>

#include "cloud/status.hpp"

namespace akira::cloud {

class LibraryStatus {
public:
    enum class Chip { Ready, Catalog, Unavailable, Warning, Empty, Pair, Checking, Error };
    enum class Glyph { Cloud, Pair, Alert };
    enum class Tone { Accent, Dim, Danger };
    enum class Action { None, Pair, Refresh };
    enum class Option { Refresh, Pair, ClearCache };

    LibraryStatus() = default;
    explicit LibraryStatus(const Status& status);

    auto chip() const -> std::optional<Chip>;
    auto glyph() const -> Glyph;
    auto tone() const -> Tone;
    auto action() const -> Action;
    auto options() const -> std::vector<Option>;
    bool emptyLibrary() const { return availability == Availability::Empty; }
    bool checking() const { return availability == Availability::Checking; }
    bool canPair() const { return pairable; }

private:
    Availability availability = Availability::NoProfile;
    bool pairable = false;
};

}
