#include "ui/theme.hpp"

#include <borealis.hpp>

namespace akira::ui
{
static const Palette kPlayStation = {
    .id              = "playstation",
    .name            = "akira/themes/cobalt",
    .background      = nvgRGB(0x0b, 0x1a, 0x3a),
    .backgroundDeep  = nvgRGB(0x08, 0x0f, 0x20),
    .gradientTop     = nvgRGB(0x18, 0x33, 0x6e),
    .gradientBottom  = nvgRGB(0x05, 0x08, 0x12),
    .surface         = nvgRGB(0x16, 0x30, 0x7a),
    .surfaceElevated = nvgRGB(0x1e, 0x45, 0xa8),
    .surfaceLine     = nvgRGBA(0xff, 0xff, 0xff, 0x17),
    .accent          = nvgRGB(0x4c, 0x9b, 0xff),
    .accentStrong    = nvgRGB(0x2e, 0x7b, 0xf6),
    .focusA          = nvgRGB(0x6f, 0xb3, 0xff),
    .focusB          = nvgRGB(0xdc, 0xeb, 0xff),
    .text            = nvgRGB(0xff, 0xff, 0xff),
    .textMuted       = nvgRGB(0xae, 0xc2, 0xe6),
    .textDim         = nvgRGB(0x6b, 0x7c, 0xa0),
    .success         = nvgRGB(0x3b, 0xc7, 0x7a),
    .warning         = nvgRGB(0xf5, 0xa6, 0x23),
    .danger          = nvgRGB(0xe5, 0x48, 0x4d),
    .media           = nvgRGB(0xb1, 0x5c, 0xe0),
    .gold            = nvgRGB(0xe9, 0xb9, 0x49),
    .silver          = nvgRGB(0xcd, 0xd5, 0xdf),
    .bronze          = nvgRGB(0xd0, 0x8a, 0x4f),
};

static const Palette kPlayStation30Light = {
    .id              = "ps30-light",
    .name            = "akira/themes/anniversary_light",
    .background      = nvgRGB(0xe7, 0xe6, 0xdc),
    .backgroundDeep  = nvgRGB(0xdc, 0xdb, 0xd0),
    .gradientTop     = nvgRGB(0xee, 0xed, 0xe4),
    .gradientBottom  = nvgRGB(0xd8, 0xd7, 0xcc),
    .surface         = nvgRGB(0xd1, 0xd2, 0xca),
    .surfaceElevated = nvgRGB(0xc3, 0xc4, 0xbb),
    .surfaceLine     = nvgRGBA(0x2a, 0x2b, 0x24, 0x26),
    .accent          = nvgRGB(0x4f, 0x83, 0xcf),
    .accentStrong    = nvgRGB(0x3f, 0x6f, 0xb8),
    .focusA          = nvgRGB(0x8f, 0x90, 0x89),
    .focusB          = nvgRGB(0xcf, 0xd0, 0xc8),
    .text            = nvgRGB(0x2c, 0x2d, 0x28),
    .textMuted       = nvgRGB(0x5c, 0x5d, 0x54),
    .textDim         = nvgRGB(0x84, 0x85, 0x7b),
    .success         = nvgRGB(0x3f, 0x9a, 0x6e),
    .warning         = nvgRGB(0xc7, 0x94, 0x12),
    .danger          = nvgRGB(0xcf, 0x4a, 0x54),
    .media           = nvgRGB(0xb9, 0x5a, 0x90),
    .gold            = nvgRGB(0xc7, 0x94, 0x12),
    .silver          = nvgRGB(0x9a, 0x9b, 0x92),
    .bronze          = nvgRGB(0xb0, 0x70, 0x3a),
};

static const Palette kPlayStation30 = {
    .id              = "ps30",
    .name            = "akira/themes/anniversary_dark",
    .background      = nvgRGB(0x2b, 0x2b, 0x2e),
    .backgroundDeep  = nvgRGB(0x1b, 0x1b, 0x1e),
    .gradientTop     = nvgRGB(0x3a, 0x3a, 0x3e),
    .gradientBottom  = nvgRGB(0x16, 0x16, 0x18),
    .surface         = nvgRGB(0x37, 0x37, 0x3b),
    .surfaceElevated = nvgRGB(0x45, 0x45, 0x4a),
    .surfaceLine     = nvgRGBA(0xff, 0xff, 0xff, 0x1c),
    .accent          = nvgRGB(0x5b, 0x8f, 0xd6),
    .accentStrong    = nvgRGB(0x45, 0x76, 0xc0),
    .focusA          = nvgRGB(0xe0, 0xe0, 0xd8),
    .focusB          = nvgRGB(0xa9, 0xaa, 0xa3),
    .text            = nvgRGB(0xf0, 0xef, 0xe9),
    .textMuted       = nvgRGB(0xb6, 0xb5, 0xac),
    .textDim         = nvgRGB(0x7c, 0x7c, 0x74),
    .success         = nvgRGB(0x57, 0xb9, 0x8c),
    .warning         = nvgRGB(0xf0, 0xc2, 0x4a),
    .danger          = nvgRGB(0xe0, 0x5a, 0x63),
    .media           = nvgRGB(0xcf, 0x6f, 0xa5),
    .gold            = nvgRGB(0xe8, 0xbd, 0x52),
    .silver          = nvgRGB(0xcd, 0xcc, 0xc4),
    .bronze          = nvgRGB(0xcf, 0x8a, 0x4f),
};

static const Palette* const kThemes[] = {
    &kPlayStation,
    &kPlayStation30,
    &kPlayStation30Light,
};

static const Palette* g_active = &kPlayStation;

const Palette& active()
{
    return *g_active;
}

bool setActiveTheme(std::string_view id)
{
    for (const Palette* theme : kThemes)
    {
        if (theme->id == id)
        {
            g_active = theme;
            return true;
        }
    }
    return false;
}

int themeCount()
{
    return static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
}

const Palette& themeAt(int index)
{
    return *kThemes[index];
}

NVGcolor withAlpha(NVGcolor color, unsigned char alpha)
{
    return nvgRGBA(
        static_cast<unsigned char>(color.r * 255.0f),
        static_cast<unsigned char>(color.g * 255.0f),
        static_cast<unsigned char>(color.b * 255.0f),
        alpha);
}

void applyToBorealis()
{
    const Palette& p = active();

    auto set = [](const char* key, NVGcolor color) {
        brls::Theme::getLightTheme().addColor(key, color);
        brls::Theme::getDarkTheme().addColor(key, color);
    };

    set("brls/background", p.background);
    set("akira/gradient_top", p.gradientTop);
    set("akira/gradient_bottom", p.gradientBottom);
    set("brls/text", p.text);
    set("brls/text_disabled", p.textDim);
    set("brls/click_pulse", withAlpha(p.accent, 0x26));
    set("akira/text_muted", p.textMuted);
    set("akira/accent", p.accent);
    set("akira/surface_line", p.surfaceLine);

    set("brls/applet_frame/separator", p.surfaceLine);
    set("brls/header/border", p.surfaceLine);
    set("brls/header/rectangle", p.textMuted);
    set("brls/header/subtitle", p.textDim);

    set("brls/button/primary_enabled_background", p.accentStrong);
    set("brls/button/primary_disabled_background", p.surfaceLine);
    set("brls/button/primary_enabled_text", p.text);
    set("brls/button/primary_disabled_text", p.textDim);
    set("brls/button/default_enabled_background", p.surface);
    set("brls/button/default_disabled_background", p.surface);
    set("brls/button/default_enabled_text", p.text);
    set("brls/button/default_disabled_text", p.textDim);
    set("brls/button/highlight_enabled_text", p.accent);
    set("brls/button/highlight_disabled_text", p.accent);
    set("brls/button/enabled_border_color", p.surfaceLine);
    set("brls/button/disabled_border_color", p.surfaceLine);

    set("brls/slider/line_filled", p.accentStrong);
    set("brls/slider/line_empty", p.surfaceLine);
    set("brls/slider/pointer_color", p.text);
    set("brls/slider/pointer_border_color", p.accent);

    set("brls/sidebar/background", p.backgroundDeep);
    set("brls/sidebar/active_item", p.accent);
    set("brls/sidebar/separator", p.surfaceLine);

    set("brls/list/listItem_value_color", p.accent);

    set("brls/highlight/color1", p.focusA);
    set("brls/highlight/color2", p.focusB);
    set("brls/highlight/background", p.background);

    bool logoSelector = (p.id == "ps30" || p.id == "ps30-light");
    auto glowOr = [&](NVGcolor c) { return logoSelector ? c : p.focusB; };
    set("akira/highlight/multiglow", nvgRGBA(0, 0, 0, logoSelector ? 0xff : 0x00));
    set("akira/highlight/glow1", glowOr(p.success));
    set("akira/highlight/glow2", glowOr(p.danger));
    set("akira/highlight/glow3", glowOr(p.accent));
    set("akira/highlight/glow4", glowOr(p.media));
    set("akira/highlight/ribbon1", glowOr(p.warning));
    set("akira/highlight/ribbon2", glowOr(p.success));
    set("akira/highlight/ribbon3", glowOr(p.danger));
    set("akira/highlight/ribbon4", glowOr(p.accent));

    set("brls/spinner/bar_color", withAlpha(p.accent, 0x50));

    set("color/card", p.surface);
    set("color/grey_3", p.surfaceElevated);
}
}
