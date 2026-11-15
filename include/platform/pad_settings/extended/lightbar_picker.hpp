#ifndef AKIRA_LIGHTBAR_PICKER_HPP
#define AKIRA_LIGHTBAR_PICKER_HPP

#include <borealis.hpp>
#include <cstdint>
#include <functional>

class LightbarPicker : public brls::Box {
public:
    LightbarPicker(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    brls::View* getDefaultFocus() override { return this; }

    void setChangeListener(std::function<void(std::uint8_t, std::uint8_t, std::uint8_t)> fn)
    {
        m_changed = std::move(fn);
    }

private:
    void emit();
    void moveHue(float delta);
    void moveField(float ds, float dv);

    float m_h = 0.0f;
    float m_s = 1.0f;
    float m_v = 1.0f;

    bool m_on_hue = false;

    std::function<void(std::uint8_t, std::uint8_t, std::uint8_t)> m_changed;
};

#endif // AKIRA_LIGHTBAR_PICKER_HPP
