#ifndef AKIRA_CLOUD_SHORTCUTS_RAIL_HPP
#define AKIRA_CLOUD_SHORTCUTS_RAIL_HPP

#include <borealis.hpp>

#include <functional>
#include <memory>
#include <string>

#include "cloud/models.hpp"

class CloudShortcutsRail : public brls::Box {
public:
    CloudShortcutsRail();
    ~CloudShortcutsRail() override;

    void refresh();
    void setLaunchHandler(std::function<void(const cloud::Game&)> handler);
    void setLeadingCard(std::function<brls::View*()> factory);

private:
    void removeShortcut(const std::string& productId);

    brls::Label* titleLabel = nullptr;
    brls::Box* railRow = nullptr;
    std::function<void(const cloud::Game&)> launchHandler;
    std::function<brls::View*()> leadingCardFactory;
    int refreshGen = 0;

    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

#endif // AKIRA_CLOUD_SHORTCUTS_RAIL_HPP
