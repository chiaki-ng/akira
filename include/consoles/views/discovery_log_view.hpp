#ifndef AKIRA_DISCOVERY_LOG_VIEW_HPP
#define AKIRA_DISCOVERY_LOG_VIEW_HPP

#include <borealis.hpp>
#include <cstdint>

class DiscoveryLogView : public brls::Box {
public:
    DiscoveryLogView();
    ~DiscoveryLogView() override;

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    static brls::View* create();

private:
    brls::Box* logContainer = nullptr;
    brls::ScrollingFrame* scrollFrame = nullptr;
    brls::Label* statusLabel = nullptr;
    brls::Button* closeBtn = nullptr;
    brls::Button* clearBtn = nullptr;

    brls::RepeatingTimer refreshTimer;
    uint64_t lastVersion = UINT64_MAX;
    bool needsScrollToBottom = false;

    void refreshLog();
};

#endif // AKIRA_DISCOVERY_LOG_VIEW_HPP
