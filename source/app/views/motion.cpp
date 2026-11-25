#include "app/views/motion.hpp"

#include <borealis.hpp>

#include <cmath>
#include <memory>

namespace akira::app::views::motion {

void animate(brls::Animatable& track, float from, float to, int durationMs,
    brls::EasingFunction easing, std::function<void(float)> onValue,
    std::function<void()> onEnd)
{
    brls::Animatable* t = &track;
    track.reset(from);
    track.addStep(to, durationMs <= 0 ? 1 : durationMs, easing);
    track.setTickCallback([t, onValue]() { onValue(t->getValue()); });
    track.setEndCallback([to, onValue, onEnd](bool finished) {
        if (!finished)
            return;
        onValue(to);
        if (onEnd)
            onEnd();
    });
    onValue(from);
    track.start();
}

void animateDelayed(brls::Animatable& track, float from, float to, int delayMs, int durationMs,
    brls::EasingFunction easing, std::function<void(float)> onValue,
    std::function<void()> onEnd)
{
    brls::Animatable* t = &track;
    track.reset(from);
    if (delayMs > 0)
        track.addStep(from, delayMs, brls::EasingFunction::linear);
    track.addStep(to, durationMs <= 0 ? 1 : durationMs, easing);
    track.setTickCallback([t, onValue]() { onValue(t->getValue()); });
    track.setEndCallback([to, onValue, onEnd](bool finished) {
        if (!finished)
            return;
        onValue(to);
        if (onEnd)
            onEnd();
    });
    onValue(from);
    track.start();
}

void fadeIn(brls::View* view, brls::Animatable& track, int delayMs, int durationMs)
{
    animateDelayed(track, 0.0f, 1.0f, delayMs, durationMs, brls::EasingFunction::quadraticOut,
        [view](float v) { view->setAlpha(v); });
}

void riseIn(brls::View* view, brls::Animatable& track, int delayMs, int durationMs, float fromY)
{
    animateDelayed(track, 0.0f, 1.0f, delayMs, durationMs, brls::EasingFunction::quadraticOut,
        [view, fromY](float v) {
            view->setAlpha(v);
            view->setTranslationY(fromY * (1.0f - v));
        });
}

void liftOnFocus(brls::View* view, brls::Animatable& track, float lift, int durationMs)
{
    brls::Animatable* t = &track;
    view->getFocusEvent()->subscribe([view, t, lift, durationMs](brls::View*) {
        animate(*t, t->getValue(), lift, durationMs, brls::EasingFunction::quadraticOut,
            [view](float v) { view->setTranslationY(-v); });
    });
    view->getFocusLostEvent()->subscribe([view, t, durationMs](brls::View*) {
        animate(*t, t->getValue(), 0.0f, durationMs, brls::EasingFunction::quadraticOut,
            [view](float v) { view->setTranslationY(-v); });
    });
}

void fillTo(brls::Rectangle* rect, brls::Animatable& track, float fromPct, float toPct, int durationMs)
{
    animate(track, fromPct, toPct, durationMs, brls::EasingFunction::quadraticOut,
        [rect](float v) { rect->setWidthPercentage(v); });
}

void countTo(brls::Label* label, brls::Animatable& track, int from, int to, int durationMs,
    std::function<std::string(int)> fmt)
{
    animate(track, static_cast<float>(from), static_cast<float>(to), durationMs,
        brls::EasingFunction::quadraticOut,
        [label, fmt](float v) { label->setText(fmt(static_cast<int>(std::lround(v)))); });
}

void crossfade(brls::View* view, brls::Animatable& track, std::function<void()> onSwap, int durationMs)
{
    int ms = durationMs <= 0 ? 1 : durationMs;
    brls::Animatable* t = &track;
    auto swapped = std::make_shared<bool>(false);

    track.reset(1.0f);
    track.addStep(0.0f, ms, brls::EasingFunction::quadraticOut);
    track.addStep(1.0f, ms, brls::EasingFunction::quadraticIn);
    track.setTickCallback([view, t, onSwap, swapped]() {
        view->setAlpha(t->getValue());
        if (!*swapped && t->getProgress() >= 0.5f) {
            *swapped = true;
            if (onSwap)
                onSwap();
        }
    });
    track.setEndCallback([view, onSwap, swapped](bool finished) {
        if (!finished)
            return;
        if (!*swapped) {
            *swapped = true;
            if (onSwap)
                onSwap();
        }
        view->setAlpha(1.0f);
    });
    view->setAlpha(1.0f);
    track.start();
}

LiftBox::LiftBox(float lift, int durationMs)
{
    liftOnFocus(this, liftAnim, lift, durationMs);
}

}
