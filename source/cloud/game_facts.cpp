#include "cloud/game_facts.hpp"

#include <ctime>

namespace akira::cloud {

GameFacts::GameFacts(const GameDetail& detail)
{
    if (!detail.publisher.empty())
        shown.push_back({Fact::Publisher, detail.publisher});
    if (detail.releasedMs)
        shown.push_back({Fact::Released, releaseDate(*detail.releasedMs)});
    if (!detail.genres.empty())
    {
        std::string genres;
        for (const std::string& genre : detail.genres)
            genres += (genres.empty() ? "" : ", ") + genre;
        shown.push_back({Fact::Genres, genres});
    }
}

auto GameFacts::releaseDate(int64_t releasedMs) -> std::string
{
    const std::time_t at = static_cast<std::time_t>(releasedMs / 1000);
    std::tm utc = *std::gmtime(&at);
    char text[64];
    std::strftime(text, sizeof(text), "%d %B %Y", &utc);
    return text;
}

}
