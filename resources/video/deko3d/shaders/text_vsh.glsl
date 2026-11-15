#version 460

// Simple text/overlay vertex shader
// Vertices are pre-transformed to NDC in the application

layout (location = 0) in vec3 inPosition;   // NDC position (x, y, z)
layout (location = 1) in vec2 inTexCoord;   // Texture UV or unused for solid
layout (location = 2) in vec4 inColor;      // Vertex color (RGBA)

layout (location = 0) out vec2 vTexCoord;
layout (location = 1) out vec4 vColor;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    vTexCoord = inTexCoord;
    vColor = inColor;
}
