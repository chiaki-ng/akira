#pragma once

#include <borealis.hpp>

#include <functional>
#include <string>
#include <vector>

#include "cloud/game_badges.hpp"
#include "cloud/models.hpp"

namespace akira::cloud::views {

class CloudGameCard : public brls::Box {
public:
    CloudGameCard();
    ~CloudGameCard() override;

    void bind(const Game& game, const std::function<void(const Game&)>& onLaunch,
        const std::function<void(const Game&)>& onPin);
    void reset();

    std::string iconUrl;
    brls::Image* cover = nullptr;
    brls::Label* name = nullptr;
    brls::Box* pillRow = nullptr;

private:
    static brls::Box* makePill(const std::string& text, NVGcolor color);
    static brls::Box* pillFor(const GameBadges::Badge& badge);

    static inline std::vector<CloudGameCard*> live;

    Game boundGame;
    std::function<void(const Game&)> launchCb;
    std::function<void(const Game&)> pinCb;
    brls::Animatable focusAnim{0.0f};
};

}
