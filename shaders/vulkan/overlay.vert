#version 450

layout(push_constant) uniform OverlayRect
{
    vec4 rect;
    vec4 uvScale;
};

layout(location = 0) out vec2 vTexCoord;

void main()
{
    vec2 corner = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    gl_Position = vec4(mix(rect.xy, rect.zw, corner), 0.0, 1.0);
    vTexCoord = corner * uvScale.xy;
}
