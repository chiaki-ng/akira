#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace akira::consoles {

class RecentPlay {
public:
    using Clock = std::chrono::system_clock;

    struct Title {
        std::string titleId;
        std::string name;
        std::string coverUrl;
        std::string conceptId;
        bool ps5 = true;
        std::optional<Clock::time_point> lastUsed;
        bool recentlyPlayed = false;

        friend bool operator==(const Title&, const Title&) = default;
    };

    enum class Span { Today, Yesterday, Days, Weeks, Months, Years };

    struct Age {
        Span span = Span::Today;
        int count = 0;

        friend bool operator==(const Age&, const Age&) = default;
    };

    static constexpr std::size_t limit = 12;

    explicit RecentPlay(std::vector<Title> titles);

    auto titles() const -> const std::vector<Title>& { return chosen; }
    auto find(const std::string& titleId) const -> std::optional<Title>;

    static auto age(Clock::time_point used, Clock::time_point now) -> Age;
    static auto ageKey(Span span) -> const char*;

private:
    std::vector<Title> chosen;
};

}
