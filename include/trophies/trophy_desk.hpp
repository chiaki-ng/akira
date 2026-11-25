#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <libchiaking/trophies_types.hpp>

#include "accounts/status.hpp"

namespace akira::trophies {

class TrophyDesk {
public:
    using Title = libchiaking::trophies::TrophyTitle;
    using Summary = libchiaking::trophies::TrophySummary;
    using Detail = libchiaking::trophies::TitleTrophies;
    using TitleId = libchiaking::trophies::NpCommunicationId;

    template <typename Value>
    using Answer = std::function<void(std::expected<Value, accounts::Status>)>;

    struct Played {
        std::optional<std::chrono::seconds> time;
        std::optional<std::chrono::system_clock::time_point> last;

        friend bool operator==(const Played&, const Played&) = default;
    };

    static constexpr std::string_view libraryScope = "library";

    virtual ~TrophyDesk() = default;

    virtual auto linked() const -> bool = 0;
    virtual auto enabled() const -> bool = 0;

    virtual void summary(bool force, Answer<Summary> done) = 0;
    virtual void library(bool force, Answer<std::vector<Title>> done) = 0;
    virtual void detail(const Title& title, bool force, Answer<Detail> done) = 0;

    virtual auto played(const TitleId& title) const -> std::optional<Played> = 0;
    virtual void resolvePlayed(const std::vector<Title>& titles, std::function<void()> updated) = 0;

    virtual auto cooldown(std::string_view scope) -> std::optional<std::chrono::seconds> = 0;
    virtual void recordRefresh(std::string_view scope) = 0;
    virtual auto savedAt() const -> std::int64_t = 0;
};

}
