#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <arisa/composition/observable.hpp>

#include "app/notifier.hpp"

namespace akira::app::arisa {

class Notices final : public Notifier {
public:
    struct Notice {
        std::uint64_t serial = 0;
        std::string text;

        friend bool operator==(const Notice&, const Notice&) = default;
    };

    void notify(std::string text) override;
    void clear();

    auto view() const -> const ::arisa::composition::Observable<std::optional<Notice>>& { return state; }

private:
    std::uint64_t serial = 0;
    ::arisa::composition::Observable<std::optional<Notice>> state;
};

}

template <>
struct std::hash<akira::app::arisa::Notices::Notice> {
    auto operator()(const akira::app::arisa::Notices::Notice& notice) const noexcept -> std::size_t
    {
        return std::hash<std::uint64_t>{}(notice.serial);
    }
};
