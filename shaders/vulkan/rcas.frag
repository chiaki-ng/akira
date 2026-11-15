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

#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform RcasParams
{
    uvec4 rcasCon0;
};

float APrxMedRcpF1(float a)
{
    float b = uintBitsToFloat(0x7ef19fffu - floatBitsToUint(a));
    return b * (-b * a + 2.0);
}

float AMin3F1(float x, float y, float z) { return min(x, min(y, z)); }
float AMax3F1(float x, float y, float z) { return max(x, max(y, z)); }

vec3 fetchClamped(ivec2 p)
{
    return texelFetch(inputTexture, clamp(p, ivec2(0), textureSize(inputTexture, 0) - 1), 0).rgb;
}

void main()
{
    ivec2 sp = ivec2(gl_FragCoord.xy);

    vec3 b = fetchClamped(sp + ivec2( 0, -1));
    vec3 d = fetchClamped(sp + ivec2(-1,  0));
    vec3 e = fetchClamped(sp);
    vec3 f = fetchClamped(sp + ivec2( 1,  0));
    vec3 h = fetchClamped(sp + ivec2( 0,  1));

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
    float lobe = max(-FSR_RCAS_LIMIT, min(AMax3F1(lobeR, lobeG, lobeB), 0.0)) * uintBitsToFloat(rcasCon0.x);

    lobe *= nz;

    float rcpL = APrxMedRcpF1(4.0 * lobe + 1.0);
    float pixR = (lobe * b.r + lobe * d.r + lobe * h.r + lobe * f.r + e.r) * rcpL;
    float pixG = (lobe * b.g + lobe * d.g + lobe * h.g + lobe * f.g + e.g) * rcpL;
    float pixB = (lobe * b.b + lobe * d.b + lobe * h.b + lobe * f.b + e.b) * rcpL;

    outColor = vec4(pixR, pixG, pixB, 1.0);
}
