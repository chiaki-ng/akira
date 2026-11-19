#ifndef AKIRA_UI_MOTION_HPP
#define AKIRA_UI_MOTION_HPP

#include <borealis/core/animation.hpp>
#include <borealis/core/box.hpp>

#include <functional>
#include <string>

namespace brls {
class View;
class Label;
class Rectangle;
}

namespace akira::ui::motion {

void animate(brls::Animatable& track, float from, float to, int durationMs,
    brls::EasingFunction easing, std::function<void(float)> onValue,
    std::function<void()> onEnd = {});

void animateDelayed(brls::Animatable& track, float from, float to, int delayMs, int durationMs,
    brls::EasingFunction easing, std::function<void(float)> onValue,
    std::function<void()> onEnd = {});

void fadeIn(brls::View* view, brls::Animatable& track, int delayMs = 0, int durationMs = 300);

void riseIn(brls::View* view, brls::Animatable& track, int delayMs = 0, int durationMs = 320,
    float fromY = 28.0f);

void liftOnFocus(brls::View* view, brls::Animatable& track, float lift = 8.0f, int durationMs = 140);

void fillTo(brls::Rectangle* rect, brls::Animatable& track, float fromPct, float toPct, int durationMs);

void countTo(brls::Label* label, brls::Animatable& track, int from, int to, int durationMs,
    std::function<std::string(int)> fmt);

void crossfade(brls::View* view, brls::Animatable& track, std::function<void()> onSwap,
    int durationMs = 140);

class LiftBox : public brls::Box {
public:
    explicit LiftBox(float lift = 8.0f, int durationMs = 140);

private:
    brls::Animatable liftAnim{0.0f};
};

}

#endif // AKIRA_UI_MOTION_HPP
