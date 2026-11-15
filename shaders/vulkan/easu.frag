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

#version 450
#extension GL_GOOGLE_include_directive : require

#include "nv12.glsl"

#define FSR_EASU_EARLY_OUT_THRESHOLD (8.0 / 255.0)

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

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

void main()
{
    uvec2 ip = uvec2(gl_FragCoord.xy);

    vec2 pp = vec2(ip) * uintBitsToFloat(easuCon0.xy) + uintBitsToFloat(easuCon0.zw);
    vec2 fp = floor(pp);
    pp -= fp;

    vec2 rcpSize = uintBitsToFloat(easuCon1.xy);

    vec4 gA = gatherLuma(fp, rcpSize);
    vec4 gB = gatherLuma(fp + vec2(2.0, 0.0), rcpSize);
    vec4 gC = gatherLuma(fp + vec2(0.0, 2.0), rcpSize);
    vec4 gD = gatherLuma(fp + vec2(2.0, 2.0), rcpSize);

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
    vec3 rgb = decodeYuv(finalY, sampleChroma(chromaUV));
    outColor = vec4(applyDither(rgb), 1.0);
}
