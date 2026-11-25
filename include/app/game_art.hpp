#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "app/art_cache.hpp"
#include "app/art_source.hpp"

class HttpPool;
class HttpSession;

namespace akira::app {

class UiDispatcher;

class GameArt final : public ArtSource {
public:
    GameArt(HttpPool& http, UiDispatcher& dispatcher);

    GameArt(const GameArt&) = delete;
    GameArt& operator=(const GameArt&) = delete;

    void fetch(std::string url, Delivery done) override;
    void fetch(std::string url, ArtCache::Shelf shelf, Delivery done);
    void discard(const std::string& url);
    void clear(ArtCache::Shelf shelf);

private:
    static constexpr long timeoutSeconds = 20;

    struct Waiting {
        ArtCache::Shelf shelf;
        std::vector<Delivery> deliveries;
    };

    void download(HttpSession& session, const std::string& url, ArtCache::Shelf shelf);
    auto downloaded(HttpSession& session, const std::string& url, ArtCache::Shelf shelf) -> std::optional<ArtCache::Bytes>;
    void deliver(const std::string& url, std::optional<Bytes> bytes);
    static auto converted(const ArtCache::Bytes& raw) -> Bytes;

    HttpPool& http;
    UiDispatcher& dispatcher;
    ArtCache cache;
    std::mutex mutex;
    std::map<std::string, Waiting, std::less<>> waiting;
};

}
