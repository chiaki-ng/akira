#pragma once

namespace akira::play {

struct UpscalingSummary {
    bool active = false;
    bool easu = false;
    bool rcas = false;
    bool supersampling = false;
    float sharpness = 0.2f;
    int inputHeight = 0;
    int targetHeight = 0;
    int displayHeight = 0;

    friend bool operator==(const UpscalingSummary&, const UpscalingSummary&) = default;
};

}
