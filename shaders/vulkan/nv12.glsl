layout(constant_id = 0) const bool DITHER = false;
layout(constant_id = 1) const float DITHER_STRENGTH = 1.0;

layout(push_constant) uniform VideoParams
{
    uvec4 easuCon0;
    uvec4 easuCon1;
    uvec4 easuCon2;
    uvec4 easuCon3;
    uvec4 planeBase;
    uvec4 planeSize;
};

#ifdef TEXEL_BUFFER
layout(set = 0, binding = 0) uniform samplerBuffer lumaTexels;
layout(set = 0, binding = 1) uniform samplerBuffer chromaTexels;

float lumaTexel(ivec2 p)
{
    p = clamp(p, ivec2(0), ivec2(planeSize.xy) - 1);
    return texelFetch(lumaTexels, int(planeBase.x) + p.y * int(planeBase.y) + p.x).r;
}

vec2 chromaTexel(ivec2 p)
{
    p = clamp(p, ivec2(0), ivec2(planeSize.zw) - 1);
    return texelFetch(chromaTexels, int(planeBase.z) + p.y * int(planeBase.w) + p.x).rg;
}

float sampleLuma(vec2 uv)
{
    vec2 t = uv * vec2(planeSize.xy) - 0.5;
    vec2 f = fract(t);
    ivec2 i = ivec2(floor(t));
    float top = mix(lumaTexel(i), lumaTexel(i + ivec2(1, 0)), f.x);
    float bottom = mix(lumaTexel(i + ivec2(0, 1)), lumaTexel(i + ivec2(1, 1)), f.x);
    return mix(top, bottom, f.y);
}

vec2 sampleChroma(vec2 uv)
{
    vec2 t = uv * vec2(planeSize.zw) - 0.5;
    vec2 f = fract(t);
    ivec2 i = ivec2(floor(t));
    vec2 top = mix(chromaTexel(i), chromaTexel(i + ivec2(1, 0)), f.x);
    vec2 bottom = mix(chromaTexel(i + ivec2(0, 1)), chromaTexel(i + ivec2(1, 1)), f.x);
    return mix(top, bottom, f.y);
}

vec4 gatherLuma(vec2 corner, vec2 rcpSize)
{
    ivec2 i = ivec2(corner) - 1;
    return vec4(lumaTexel(i + ivec2(0, 1)), lumaTexel(i + ivec2(1, 1)), lumaTexel(i + ivec2(1, 0)), lumaTexel(i));
}
#else
layout(set = 0, binding = 0) uniform sampler2D lumaPlane;
layout(set = 0, binding = 1) uniform sampler2D chromaPlane;

float sampleLuma(vec2 uv)
{
    return texture(lumaPlane, uv).r;
}

vec2 sampleChroma(vec2 uv)
{
    return texture(chromaPlane, uv).rg;
}

vec4 gatherLuma(vec2 corner, vec2 rcpSize)
{
    return textureGather(lumaPlane, corner * rcpSize);
}
#endif

vec3 decodeYuv(float y_raw, vec2 uv_raw)
{
    float y = (y_raw - 16.0/255.0) / ((235.0 - 16.0) / 255.0);
    float u = (uv_raw.r - 16.0/255.0) / ((240.0 - 16.0) / 255.0) - 0.5;
    float v = (uv_raw.g - 16.0/255.0) / ((240.0 - 16.0) / 255.0) - 0.5;

    float r = y + 1.5748 * v;
    float g = y - 0.18733 * u - 0.46812 * v;
    float b = y + 1.85563 * u;

    return clamp(vec3(r, g, b), 0.0, 1.0);
}

float interleavedGradientNoise(vec2 uv)
{
    return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
}

vec3 applyDither(vec3 rgb)
{
    if (!DITHER)
        return rgb;
    return rgb + (DITHER_STRENGTH / 255.0) * interleavedGradientNoise(gl_FragCoord.xy) - (DITHER_STRENGTH / 510.0);
}
