#pragma once

#include <borealis/extern/nanovg/nanovg.h>
#include <string_view>

namespace akira::ui
{
struct Palette
{
    std::string_view id;
    std::string_view name;

    NVGcolor background;
    NVGcolor backgroundDeep;
    NVGcolor gradientTop;
    NVGcolor gradientBottom;
    NVGcolor surface;
    NVGcolor surfaceElevated;
    NVGcolor surfaceLine;
    NVGcolor accent;
    NVGcolor accentStrong;
    NVGcolor focusA;
    NVGcolor focusB;
    NVGcolor text;
    NVGcolor textMuted;
    NVGcolor textDim;
    NVGcolor success;
    NVGcolor warning;
    NVGcolor danger;
    NVGcolor media;
    NVGcolor gold;
    NVGcolor silver;
    NVGcolor bronze;
};

const Palette& active();
bool setActiveTheme(std::string_view id);
int themeCount();
const Palette& themeAt(int index);

NVGcolor withAlpha(NVGcolor color, unsigned char alpha);

void applyToBorealis();
}
