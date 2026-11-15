#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    vTexCoord = inTexCoord;
}
