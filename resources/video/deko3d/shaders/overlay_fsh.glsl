#version 460

layout (location = 0) in vec2 vTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D overlayTexture;

void main()
{
    outColor = texture(overlayTexture, vTexCoord);
}
