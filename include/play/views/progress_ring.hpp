#ifndef AKIRA_PROGRESS_RING_HPP
#define AKIRA_PROGRESS_RING_HPP

#include <borealis.hpp>

class ProgressRing : public brls::Box {
public:
    ProgressRing();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

    void setProgress(float ratio);
    void setThickness(float px);

private:
    float progress = 0.0f;
    float thickness = 7.0f;
    brls::Animatable displayProgress{0.0f};
};

#endif // AKIRA_PROGRESS_RING_HPP
