#pragma once

#include <array>

namespace akira::app {

enum class RailEdge { Left, Top, Right, Bottom };

inline constexpr std::array railEdges{ RailEdge::Left, RailEdge::Top, RailEdge::Right, RailEdge::Bottom };

}
