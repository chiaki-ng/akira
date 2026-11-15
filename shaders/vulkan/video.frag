#version 450
#extension GL_GOOGLE_include_directive : require

#include "nv12.glsl"

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 rgb = decodeYuv(sampleLuma(vTexCoord), sampleChroma(vTexCoord));
    outColor = vec4(applyDither(rgb), 1.0);
}
