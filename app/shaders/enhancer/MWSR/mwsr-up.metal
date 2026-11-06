// MWSR — MoonlightWeb Super Resolution: the upscale, Metal Shading Language.
// Copyright (c) 2026 Bruno Martin <brunoocto@gmail.com>
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// The kernel of mwsr-up.hlsl line for line, from https://github.com/linckosz/moonlight-web/tree/main/mwsr.
// Its constants are mwsr::constants() (mwsr.h) at buffer(1), the RGB picture at texture(0), drawn by
// vt_renderer.metal's vs_draw. MWSR_CHROMA444 1 (a preprocessor macro) for a 4:4:4 stream.
#include <metal_stdlib>
using namespace metal;

#ifndef MWSR_CHROMA444
#define MWSR_CHROMA444 0
#endif

// vs_draw's output, as vt_renderer.metal declares it
struct Vertex
{
    float4 position [[ position ]];
    float2 texCoords;
};

// mwsr::Constants, 24 floats
struct MwsrConstants
{
    float2 texel;
    float2 scale;
    float2 dims;
    float2 across;
    float kc;
    float ft;
    float kg;
    float kwc;
    float2 zc;
    float2 zr;
    float zring;
    float zft;
    float zkg;
    float gaze;
    float kl;
    float zref;
    float pad1;
    float pad2;
};

constexpr sampler mwsrSampler(coord::normalized, address::clamp_to_edge, filter::linear);

constant float3 MWSR_LUMA = float3(0.25, 0.625, 0.125);

static float3 mwsrS(texture2d<float> tex, float2 uv)
{
    return tex.sample(mwsrSampler, uv, level(0.0)).rgb;
}

fragment float4 mwsr_up(Vertex v [[ stage_in ]],
                        constant MwsrConstants &c [[ buffer(1) ]],
                        texture2d<float> tex [[ texture(0) ]])
{
    float2 uv = v.texCoords;
    float2 sp = uv * c.dims;
    float2 f0 = floor(sp - 0.5);
    float2 t = sp - 0.5 - f0;
    float2 s = 1.0 - t;
    float2 ku = c.kc * t * s;
    float2 w12 = 1.0 + ku;
    float2 p12 = (f0 + 0.5 + (t * t * (3.0 - 2.0 * t) + ku * s) / w12) * c.texel;
    float3 rM = mwsrS(tex, p12);
    // the slope per input texel along the texture's own axes, whichever way the target's rows run
    float2 dir = sign(float2(dfdx(uv.x), dfdy(uv.y))) * c.scale;
    float2 gM;
    bool isFlat;
#if MWSR_CHROMA444
    float3 dx = dfdx(rM) * dir.x;
    float3 dy = dfdy(rM) * dir.y;
    float3 m = abs(dx) + abs(dy);
    bool rg = m.g > m.r;
    bool b = m.b > max(m.r, m.g);
    gM = float2(b ? dx.b : (rg ? dx.g : dx.r), b ? dy.b : (rg ? dy.g : dy.r));
    isFlat = max(m.r, max(m.g, m.b)) < c.ft;
#else
    float lM = dot(rM, MWSR_LUMA);
    gM = float2(dfdx(lM), dfdy(lM)) * dir;
    isFlat = abs(gM.x) + abs(gM.y) < c.ft;
#endif
    if (isFlat) {
        return float4(rM, 1.0);
    }
    float2 p0 = (f0 - 0.5) * c.texel;
    float2 p3 = (f0 + 2.5) * c.texel;
    float3 rT = mwsrS(tex, float2(p12.x, p0.y));
    float3 rL = mwsrS(tex, float2(p0.x, p12.y));
    float3 rR = mwsrS(tex, float2(p3.x, p12.y));
    float3 rB = mwsrS(tex, float2(p12.x, p3.y));
    float2 dn = gM * rsqrt(dot(gM, gM)) * c.across;
    float3 ra = mwsrS(tex, uv - dn);
    float3 rb = mwsrS(tex, uv + dn);
    float3 K = (w12.x * (w12.y * rM - ku.y * (s.y * rT + t.y * rB)) - w12.y * ku.x * (s.x * rL + t.x * rR)) / (1.0 - ku.x * ku.y);
    float3 D = K - 0.5 * (ra + rb);
    float3 R = K + c.kg * D + c.kwc * (D - float3(dot(D, MWSR_LUMA)));
    float3 mn = min(rM, min(ra, rb));
    float3 mx = max(rM, max(ra, rb));
    float3 mg = (mx - mn) * c.kl;
    return float4(clamp(R, max(mn - mg, float3(0.0)), mx + mg), 1.0);
}
