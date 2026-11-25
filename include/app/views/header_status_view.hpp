#pragma once

#include <string>

#include <borealis.hpp>

namespace akira::app::views {

class HeaderStatusView : public brls::Box {
public:
    HeaderStatusView();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

private:
    brls::Label* timeLabel = nullptr;
    std::string lastTime;
};

}
