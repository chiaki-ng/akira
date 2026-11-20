#include "app/game_art.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "app/ui_dispatcher.hpp"
#include "log/log.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"

namespace akira::app {

GameArt::GameArt(HttpPool& http, UiDispatcher& dispatcher)
    : http(http)
    , dispatcher(dispatcher)
{
}

void GameArt::fetch(std::string url, Delivery done)
{
    if (url.empty())
    {
        dispatcher.runOnWorker([done = std::move(done)] { done(std::nullopt); });
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto [entry, first] = waiting.try_emplace(url);
        entry->second.push_back(std::move(done));
        if (!first)
            return;
    }
    http.submit([this, url](HttpSession& session) { download(session, url); });
}

void GameArt::download(HttpSession& session, const std::string& url)
{
    if (auto remembered = cache.remembered(url))
    {
        deliver(url, converted(*remembered));
        return;
    }
    if (auto stored = cache.load(url))
    {
        cache.remember(url, *stored);
        deliver(url, converted(*stored));
        return;
    }
    std::optional<trophies::IconCache::Bytes> fetched = downloaded(session, url);
    if (!fetched)
    {
        deliver(url, std::nullopt);
        return;
    }
    cache.remember(url, *fetched);
    cache.save(url, *fetched);
    deliver(url, converted(*fetched));
}

auto GameArt::downloaded(HttpSession& session, const std::string& url) -> std::optional<trophies::IconCache::Bytes>
{
    const HttpResponse response = session.get(url, "", timeoutSeconds);
    if (response.transportFailed())
    {
        log::warning("[ART] {} failed: {}", url, response.error);
        return std::nullopt;
    }
    if (response.status != 200)
    {
        log::warning("[ART] {} returned HTTP {}", url, response.status);
        return std::nullopt;
    }
    if (response.body.empty() || response.body.size() > artLimit)
    {
        log::warning("[ART] {} is {} bytes, outside 1..{}", url, response.body.size(), artLimit);
        return std::nullopt;
    }
    return trophies::IconCache::Bytes(response.body.begin(), response.body.end());
}

void GameArt::deliver(const std::string& url, std::optional<Bytes> bytes)
{
    std::vector<Delivery> waiters;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto entry = waiting.find(url);
        if (entry == waiting.end())
            return;
        waiters = std::move(entry->second);
        waiting.erase(entry);
    }
    dispatcher.runOnWorker([waiters = std::move(waiters), bytes = std::move(bytes)] {
        for (const Delivery& waiter : waiters)
            waiter(bytes);
    });
}

auto GameArt::converted(const trophies::IconCache::Bytes& raw) -> Bytes
{
    Bytes bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(), [](std::uint8_t value) { return std::byte{ value }; });
    return bytes;
}

}
