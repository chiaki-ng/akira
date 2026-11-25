#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/services.hpp"
#include "trophies/trophy_desk.hpp"

namespace akira::app::arisa {

class PsnTrophyDesk final : public trophies::TrophyDesk {
public:
    explicit PsnTrophyDesk(Services& services);

    PsnTrophyDesk(const PsnTrophyDesk&) = delete;
    PsnTrophyDesk& operator=(const PsnTrophyDesk&) = delete;

    auto linked() const -> bool override;
    auto enabled() const -> bool override;

    void summary(bool force, Answer<Summary> done) override;
    void library(bool force, Answer<std::vector<Title>> done) override;
    void detail(const Title& title, bool force, Answer<Detail> done) override;

    auto played(const TitleId& title) const -> std::optional<Played> override;
    void resolvePlayed(const std::vector<Title>& titles, std::function<void()> updated) override;

    auto cooldown(std::string_view scope) -> std::optional<std::chrono::seconds> override;
    void recordRefresh(std::string_view scope) override;
    auto savedAt() const -> std::int64_t override;

private:
    template <typename Value>
    static auto failed(Answer<Value> done) -> std::function<void(accounts::Status, const std::string&)>;

    Services& services;
};

}
