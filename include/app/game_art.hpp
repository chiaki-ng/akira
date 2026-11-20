#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "app/art_source.hpp"
#include "trophies/icon_cache.hpp"

class HttpPool;
class HttpSession;

namespace akira::app {

class UiDispatcher;

class GameArt final : public ArtSource {
public:
    static constexpr const char* directory = "sdmc:/switch/akira/cache/art";

    GameArt(HttpPool& http, UiDispatcher& dispatcher);

    GameArt(const GameArt&) = delete;
    GameArt& operator=(const GameArt&) = delete;

    void fetch(std::string url, Delivery done) override;

private:
    static constexpr std::size_t memoryLimit = 4 * 1024 * 1024;
    static constexpr std::size_t artLimit = 4 * 1024 * 1024;
    static constexpr long timeoutSeconds = 20;

    void download(HttpSession& session, const std::string& url);
    auto downloaded(HttpSession& session, const std::string& url) -> std::optional<trophies::IconCache::Bytes>;
    void deliver(const std::string& url, std::optional<Bytes> bytes);
    static auto converted(const trophies::IconCache::Bytes& raw) -> Bytes;

    HttpPool& http;
    UiDispatcher& dispatcher;
    trophies::IconCache cache{ directory, memoryLimit, artLimit };
    std::mutex mutex;
    std::map<std::string, std::vector<Delivery>, std::less<>> waiting;
};

}
