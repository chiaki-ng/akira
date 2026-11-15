#version 460

// Simple text/overlay fragment shader
// Uses vertex color with optional texture sampling

layout (location = 0) in vec2 vTexCoord;
layout (location = 1) in vec4 vColor;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D fontTexture;

void main()
{
    // Check if this is a background quad (UV coordinates set to -1, -1)
    // Background quads use UV outside the normal 0-1 range
    if (vTexCoord.x < 0.0)
    {
        // Background mode - use vertex color directly (semi-transparent black)
        outColor = vColor;
    }
    else
    {
        // Text mode - sample font texture and modulate alpha
        // Font texture is R8 format, red channel contains the glyph alpha
        float glyphAlpha = texture(fontTexture, vTexCoord).r;

        // Output vertex color RGB with alpha modulated by glyph
        // This makes transparent parts of characters actually transparent
        outColor = vec4(vColor.rgb, vColor.a * glyphAlpha);
    }
}
