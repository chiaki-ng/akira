#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 vTexCoord;

layout (binding = 0, std140) uniform OverlayRect
{
    vec4 rect;
    vec4 uvScale;
};

void main()
{
    vec2 pos = mix(rect.xy, rect.zw, inTexCoord);
    gl_Position = vec4(pos, 0.0, 1.0);
    vTexCoord = inTexCoord * uvScale.xy;
}
