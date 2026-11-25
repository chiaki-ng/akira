#ifndef AKIRA_CONNECTION_STAGE_HPP
#define AKIRA_CONNECTION_STAGE_HPP

#include <string>

struct NVGcontext;

namespace akira::play::views {

void drawConnectionRing(NVGcontext* vg, float cx, float cy, const std::string& label, int stageIndex, int stageTotal);
void drawConnectionFailure(NVGcontext* vg, float cx, float cy, const std::string& message);

}

#endif // AKIRA_CONNECTION_STAGE_HPP
