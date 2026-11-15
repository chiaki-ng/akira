#version 460

layout (location = 0) in vec2 vTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0, std140) uniform BorderParams
{
    vec4 color;
    vec4 geom;
};

void main()
{
    vec2 p = vTexCoord * geom.xy;
    float d = min(min(p.x, geom.x - p.x), min(p.y, geom.y - p.y));
    float e = clamp(1.0 - d / geom.z, 0.0, 1.0);
    float a = color.a * e;
    outColor = vec4(color.rgb * a, a);
}
