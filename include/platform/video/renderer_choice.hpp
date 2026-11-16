#pragma once

#include "core/renderer_kind.hpp"

namespace akira::platform
{

struct RendererChoice
{
    RendererKind kind;
    const char* label;
};

}
