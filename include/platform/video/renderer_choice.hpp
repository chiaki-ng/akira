#pragma once

#include "play/renderer_kind.hpp"

namespace akira::platform
{

struct RendererChoice
{
    play::RendererKind kind;
    const char* label;
};

}
