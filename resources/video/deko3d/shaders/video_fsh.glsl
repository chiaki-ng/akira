// FSR (FidelityFX Super Resolution) portions of this shader:
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#version 460

layout (location = 0) in vec2 vTexCoord;
layout (location = 0) out vec4 outColor;

#if defined(FSR_RCAS) || defined(FSR_PASS)
layout (binding = 0) uniform sampler2D inputTexture;
#else
layout (binding = 0) uniform sampler2D plane0;
layout (binding = 1) uniform sampler2D plane1;
#endif

#ifdef FSR_EASU
#define FSR_EASU_EARLY_OUT_THRESHOLD (8.0 / 255.0)
layout (binding = 0, std140) uniform FsrEasuConstants {
    uvec4 Const0;
    uvec4 Const1;
    uvec4 Const2;
    uvec4 Const3;
};
#endif

#ifdef FSR_RCAS
layout (binding = 0, std140) uniform FsrRcasConstants {
    uvec4 Const0;
};

#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))
#endif

#ifdef DITHER_NOISE
float interleavedGradientNoise(vec2 uv) {
    return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
}

vec3 applyDither(vec3 rgb)
{
    rgb += (DITHER_STRENGTH / 255.0) * interleavedGradientNoise(gl_FragCoord.xy) - (DITHER_STRENGTH / 510.0);
    return rgb;
}
#endif

#ifdef FSR_EASU
float APrxLoRcpF1(float a) { return uintBitsToFloat(0x7ef07ebbu - floatBitsToUint(a)); }
float APrxLoRsqF1(float a) { return uintBitsToFloat(0x5f347d74u - (floatBitsToUint(a) >> 1u)); }

void FsrEasuTapY(
    inout float aC,
    inout float aW,
    vec2 off,
    vec2 dir,
    vec2 len,
    float lob,
    float clp,
    float y)
{
    vec2 v;
    v.x = (off.x * dir.x) + (off.y * dir.y);
    v.y = (off.x * (-dir.y)) + (off.y * dir.x);
    v *= len;
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    float wB = (2.0 / 5.0) * d2 + (-1.0);
    float wA = lob * d2 + (-1.0);
    wB *= wB;
    wA *= wA;
    wB = (25.0 / 16.0) * wB + (-(25.0 / 16.0 - 1.0));
    float w = wB * wA;
    aC += y * w;
    aW += w;
}

void FsrEasuSet(
    inout vec2 dir,
    inout float len,
    float w,
    float lA, float lB, float lC, float lD, float lE)
{
    float dc = lD - lC;
    float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = APrxLoRcpF1(lenX);
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);
    lenX *= lenX;
    len += lenX * w;
    float ec = lE - lC;
    float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = APrxLoRcpF1(lenY);
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);
    lenY *= lenY;
    len += lenY * w;
}
#endif

#ifdef FSR_RCAS
float APrxMedRcpF1(float a) {
    float b = uintBitsToFloat(0x7ef19fffu - floatBitsToUint(a));
    return b * (-b * a + 2.0);
}

float AMin3F1(float x, float y, float z) { return min(x, min(y, z)); }
float AMax3F1(float x, float y, float z) { return max(x, max(y, z)); }
#endif

#if !defined(FSR_RCAS) && !defined(FSR_PASS)
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
#endif

void main()
{
#ifdef FSR_PASS
    outColor = vec4(texture(inputTexture, vTexCoord).rgb, 1.0);
#elif defined(FSR_RCAS)
    ivec2 sp = ivec2(gl_FragCoord.xy);

    vec3 b = texelFetch(inputTexture, sp + ivec2( 0, -1), 0).rgb;
    vec3 d = texelFetch(inputTexture, sp + ivec2(-1,  0), 0).rgb;
    vec3 e = texelFetch(inputTexture, sp, 0).rgb;
    vec3 f = texelFetch(inputTexture, sp + ivec2( 1,  0), 0).rgb;
    vec3 h = texelFetch(inputTexture, sp + ivec2( 0,  1), 0).rgb;

    float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
    float dL = d.b * 0.5 + (d.r * 0.5 + d.g);
    float eL = e.b * 0.5 + (e.r * 0.5 + e.g);
    float fL = f.b * 0.5 + (f.r * 0.5 + f.g);
    float hL = h.b * 0.5 + (h.r * 0.5 + h.g);

    float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
    nz = clamp(abs(nz) * APrxMedRcpF1(AMax3F1(AMax3F1(bL, dL, eL), fL, hL) - AMin3F1(AMin3F1(bL, dL, eL), fL, hL)), 0.0, 1.0);
    nz = -0.5 * nz + 1.0;

    float mn4R = min(AMin3F1(b.r, d.r, f.r), h.r);
    float mn4G = min(AMin3F1(b.g, d.g, f.g), h.g);
    float mn4B = min(AMin3F1(b.b, d.b, f.b), h.b);
    float mx4R = max(AMax3F1(b.r, d.r, f.r), h.r);
    float mx4G = max(AMax3F1(b.g, d.g, f.g), h.g);
    float mx4B = max(AMax3F1(b.b, d.b, f.b), h.b);

    float hitMinR = mn4R / (4.0 * mx4R);
    float hitMinG = mn4G / (4.0 * mx4G);
    float hitMinB = mn4B / (4.0 * mx4B);
    float hitMaxR = (1.0 - mx4R) / (4.0 * mn4R + (-4.0));
    float hitMaxG = (1.0 - mx4G) / (4.0 * mn4G + (-4.0));
    float hitMaxB = (1.0 - mx4B) / (4.0 * mn4B + (-4.0));
    float lobeR = max(-hitMinR, hitMaxR);
    float lobeG = max(-hitMinG, hitMaxG);
    float lobeB = max(-hitMinB, hitMaxB);
    float lobe = max(-FSR_RCAS_LIMIT, min(AMax3F1(lobeR, lobeG, lobeB), 0.0)) * uintBitsToFloat(Const0.x);

    lobe *= nz;

    float rcpL = APrxMedRcpF1(4.0 * lobe + 1.0);
    float pixR = (lobe * b.r + lobe * d.r + lobe * h.r + lobe * f.r + e.r) * rcpL;
    float pixG = (lobe * b.g + lobe * d.g + lobe * h.g + lobe * f.g + e.g) * rcpL;
    float pixB = (lobe * b.b + lobe * d.b + lobe * h.b + lobe * f.b + e.b) * rcpL;

    outColor = vec4(pixR, pixG, pixB, 1.0);
#elif defined(FSR_EASU)
    uvec2 ip = uvec2(gl_FragCoord.xy);

    vec2 pp = vec2(ip) * uintBitsToFloat(Const0.xy) + uintBitsToFloat(Const0.zw);
    vec2 fp = floor(pp);
    pp -= fp;

    vec2 rcpSize = uintBitsToFloat(Const1.xy);

    vec4 gA = textureGather(plane0, fp * rcpSize);
    vec4 gB = textureGather(plane0, (fp + vec2(2.0, 0.0)) * rcpSize);
    vec4 gC = textureGather(plane0, (fp + vec2(0.0, 2.0)) * rcpSize);
    vec4 gD = textureGather(plane0, (fp + vec2(2.0, 2.0)) * rcpSize);

    float Yb = gA.z, Yc = gB.w;
    float Ye = gA.x, Yf = gA.y, Yg = gB.x, Yh = gB.y;
    float Yi = gC.w, Yj = gC.z, Yk = gD.w, Yl = gD.z;
    float Yn = gC.y, Yo = gD.x;

    float minCenterY = min(min(Yf, Yg), min(Yj, Yk));
    float maxCenterY = max(max(Yf, Yg), max(Yj, Yk));
    float centerRange = maxCenterY - minCenterY;

    float finalY;
    if (centerRange < FSR_EASU_EARLY_OUT_THRESHOLD)
    {
        float top = mix(Yf, Yg, pp.x);
        float bottom = mix(Yj, Yk, pp.x);
        finalY = mix(top, bottom, pp.y);
    }
    else
    {
        vec2 dir = vec2(0.0);
        float len = 0.0;
        FsrEasuSet(dir, len, (1.0 - pp.x) * (1.0 - pp.y), Yb, Ye, Yf, Yg, Yj);
        FsrEasuSet(dir, len, pp.x * (1.0 - pp.y),          Yc, Yf, Yg, Yh, Yk);
        FsrEasuSet(dir, len, (1.0 - pp.x) * pp.y,          Yf, Yi, Yj, Yk, Yn);
        FsrEasuSet(dir, len, pp.x * pp.y,                  Yg, Yj, Yk, Yl, Yo);

        vec2 dir2 = dir * dir;
        float dirR = dir2.x + dir2.y;
        bool zro = dirR < (1.0 / 32768.0);
        dirR = APrxLoRsqF1(dirR);
        dirR = zro ? 1.0 : dirR;
        dir.x = zro ? 1.0 : dir.x;
        dir *= vec2(dirR);

        len = len * 0.5;
        len *= len;

        float stretch = (dir.x * dir.x + dir.y * dir.y) * APrxLoRcpF1(max(abs(dir.x), abs(dir.y)));
        vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 + (-0.5) * len);
        float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
        float clp = APrxLoRcpF1(lob);

        float aC = 0.0;
        float aW = 0.0;
        FsrEasuTapY(aC, aW, vec2( 0.0,-1.0) - pp, dir, len2, lob, clp, Yb);
        FsrEasuTapY(aC, aW, vec2( 1.0,-1.0) - pp, dir, len2, lob, clp, Yc);
        FsrEasuTapY(aC, aW, vec2(-1.0, 1.0) - pp, dir, len2, lob, clp, Yi);
        FsrEasuTapY(aC, aW, vec2( 0.0, 1.0) - pp, dir, len2, lob, clp, Yj);
        FsrEasuTapY(aC, aW, vec2( 0.0, 0.0) - pp, dir, len2, lob, clp, Yf);
        FsrEasuTapY(aC, aW, vec2(-1.0, 0.0) - pp, dir, len2, lob, clp, Ye);
        FsrEasuTapY(aC, aW, vec2( 1.0, 1.0) - pp, dir, len2, lob, clp, Yk);
        FsrEasuTapY(aC, aW, vec2( 2.0, 1.0) - pp, dir, len2, lob, clp, Yl);
        FsrEasuTapY(aC, aW, vec2( 2.0, 0.0) - pp, dir, len2, lob, clp, Yh);
        FsrEasuTapY(aC, aW, vec2( 1.0, 0.0) - pp, dir, len2, lob, clp, Yg);
        FsrEasuTapY(aC, aW, vec2( 1.0, 2.0) - pp, dir, len2, lob, clp, Yo);
        FsrEasuTapY(aC, aW, vec2( 0.0, 2.0) - pp, dir, len2, lob, clp, Yn);

        finalY = min(maxCenterY, max(minCenterY, aC / aW));
    }
    vec2 chromaUV = (fp + pp + 0.5) * rcpSize;
    vec2 uv_raw = texture(plane1, chromaUV).rg;

    vec3 rgb = decodeYuv(finalY, uv_raw);
#ifdef DITHER_NOISE
    rgb = applyDither(rgb);
#endif
    outColor = vec4(rgb, 1.0);
#else
    float y_raw = texture(plane0, vTexCoord).r;
    vec2 uv_raw = texture(plane1, vTexCoord).rg;
    vec3 rgb = decodeYuv(y_raw, uv_raw);
#ifdef DITHER_NOISE
    rgb = applyDither(rgb);
#endif
    outColor = vec4(rgb, 1.0);
#endif
}
