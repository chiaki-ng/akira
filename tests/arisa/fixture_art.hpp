#pragma once

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "app/art_source.hpp"

namespace tests {

class FixtureArt final : public akira::app::ArtSource {
public:
    static constexpr std::string_view host = "https://art.test/";

    explicit FixtureArt(std::filesystem::path folder)
        : folder(std::move(folder))
    {
    }

    ~FixtureArt() override
    {
        std::lock_guard<std::mutex> lock(mutex);
        workers.clear();
    }

    FixtureArt(const FixtureArt&) = delete;
    FixtureArt& operator=(const FixtureArt&) = delete;

    static auto url(std::string_view name) -> std::string { return std::string(host) + std::string(name) + ".png"; }

    void fetch(std::string url, Delivery done) override
    {
        std::optional<Bytes> bytes = read(url);
        std::lock_guard<std::mutex> lock(mutex);
        ++asked;
        workers.emplace_back([this, bytes = std::move(bytes), done = std::move(done)] {
            done(bytes);
            delivered.fetch_add(1, std::memory_order_acq_rel);
        });
    }

    auto requests() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock(mutex);
        return asked;
    }

    auto settled() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex);
        return delivered.load(std::memory_order_acquire) == asked;
    }

private:
    auto read(std::string_view url) const -> std::optional<Bytes>
    {
        if (!url.starts_with(host))
            return std::nullopt;
        std::ifstream stream(folder / std::string(url.substr(host.size())), std::ios::binary);
        if (!stream)
            return std::nullopt;
        const std::vector<char> raw{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        Bytes bytes(raw.size());
        for (std::size_t index = 0; index < raw.size(); ++index)
            bytes[index] = static_cast<std::byte>(raw[index]);
        return bytes;
    }

    std::filesystem::path folder;
    mutable std::mutex mutex;
    std::size_t asked = 0;
    std::atomic<std::size_t> delivered{ 0 };
    std::vector<std::jthread> workers;
};

}
