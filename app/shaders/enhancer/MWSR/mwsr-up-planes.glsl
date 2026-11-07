// MWSR — MoonlightWeb Super Resolution: the upscale, drawn by the Vulkan renderer in one pass
// straight from the decoder's planes, with no RGB picture in between.
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
// The kernel of mwsr-up.hook line for line. Only mwsrS() and the position differ: each read
// takes the luma and the chroma at the same spot (bilinear, held inside the picture's crop) and
// turns them into RGB there. The YCbCr to RGB conversion is affine, so this is a bilinear read of
// an RGB picture whose chroma is interpolated straight from its own grid.
// libplacebo's colour mapping to the target follows in the same pass (plvk.cpp).
//
// Set by PlVkRenderer before this text: MWSR_HDR and MWSR_CHROMA444 (0 or 1). Bound by it:
//   mwsr_luma, mwsr_chroma    the planes, linear and clamped
//   mwsr_yuv0..2, mwsr_yuvc   the rows and offset of the YCbCr to RGB transform
//   mwsr_pt, mwsr_dims        the luma texel and the luma texture's size
//   mwsr_ymin, mwsr_ymax      the first and last luma texel centres of the crop
//   mwsr_cmin, mwsr_cmax      the same for the chroma
//   mwsr_coff                 the chroma's offset from the luma (its sample location)
//   mwsr_scale                the output's size over the input's
//   mwsr_src0, mwsr_dst0      the crop's origin (luma pixels) and the output rect's origin

// The coarse derivatives of the other texts, where the GLSL version has them.
#if __VERSION__ >= 450
#define MWSR_DX dFdxCoarse
#define MWSR_DY dFdyCoarse
#else
#define MWSR_DX dFdx
#define MWSR_DY dFdy
#endif

const vec3 MWSR_LUMA = vec3(0.25, 0.625, 0.125);

vec3 mwsrS(vec2 uv) {
    // The luma's four texels, interpolated here in full precision: a filtered read of an 8-bit
    // plane can come back with 8 bits only, where the hook read an RGB picture in floating point.
    // They are gathered at their common corner, so that the GPU's rounding of the coordinate
    // cannot pick another four than the ones the weights are for.
    vec2 q = clamp(uv, mwsr_ymin, mwsr_ymax) * mwsr_dims - 0.5;
    vec2 f = floor(q);
    vec2 t = q - f;
    vec4 g = textureGather(mwsr_luma, (f + 1.0) * mwsr_pt, 0);
    float y = mix(mix(g.w, g.z, t.x), mix(g.x, g.y, t.x), t.y);
    vec3 yuv = vec3(y, textureLod(mwsr_chroma, clamp(uv + mwsr_coff, mwsr_cmin, mwsr_cmax), 0.0).rg);
    return vec3(dot(mwsr_yuv0, yuv), dot(mwsr_yuv1, yuv), dot(mwsr_yuv2, yuv)) + mwsr_yuvc;
}

vec4 mwsrPlanes() {
    // constants() for this input drawn at this output: the law's curves over the scale's logarithm,
    // held at x1.07 and x4, the sharpness forced from x1.3333333333333333 to x2, whole from x1.07 on
    vec2 texel = mwsr_pt;
    vec2 dims = mwsr_dims;
    vec2 scale = mwsr_scale;
    float smin = min(scale.x, scale.y);
    float sl = clamp(smin, 1.07, 4.0);
    float x = log(sl);
    float force = 1.0 + (1.35 - 1.0) * clamp(log(sl / 1.3333333333333333) / log(2.0 / 1.3333333333333333), 0.0, 1.0);
    float strength = clamp((smin - 1.0) / (1.07 - 1.0), 0.0, 1.0);
    float kc = 0.6353583202388096 + x * (0.021845681633595574 + x * 0.12781268100437676);
    float kg = (-0.1288413863403059 + x * (2.5844683855070816 + x * -1.260092021482419)) * force * strength;
    float kr = 1.0071546009267338 + x * (-1.0209066704914191 + x * 0.5699605831985769);
#if MWSR_HDR
    float kl = 0.0;
#else
    float kl = (0.33416994543071277 + x * (-0.2962022126856069 + x * 0.08628272130287401)) * force;
#endif
    float ft = 0.005;
    float kwc = 0.1 * strength;
    vec2 across = kr * texel;

    // this output pixel's centre, in the luma texture
    vec2 uv = (mwsr_src0 + (gl_FragCoord.xy - mwsr_dst0) / scale) * texel;
    vec2 sp = uv * dims;
    vec2 f0 = floor(sp - 0.5);
    vec2 t = sp - 0.5 - f0;
    vec2 s = 1.0 - t;
    vec2 ku = kc * t * s;
    vec2 w12 = 1.0 + ku;
    vec2 p12 = (f0 + 0.5 + (t * t * (3.0 - 2.0 * t) + ku * s) / w12) * texel;
    vec3 rM = mwsrS(p12);
    // the slope per input texel along the texture's own axes, whichever way the target's rows run
    vec2 dir = sign(vec2(dFdx(uv.x), dFdy(uv.y))) * scale;
    vec2 gM;
    bool isFlat;
#if MWSR_CHROMA444
    vec3 dx = MWSR_DX(rM) * dir.x;
    vec3 dy = MWSR_DY(rM) * dir.y;
    vec3 m = abs(dx) + abs(dy);
    bool rg = m.g > m.r;
    bool b = m.b > max(m.r, m.g);
    gM = vec2(b ? dx.b : (rg ? dx.g : dx.r), b ? dy.b : (rg ? dy.g : dy.r));
    isFlat = max(m.r, max(m.g, m.b)) < ft;
#else
    float lM = dot(rM, MWSR_LUMA);
    gM = vec2(MWSR_DX(lM), MWSR_DY(lM)) * dir;
    isFlat = abs(gM.x) + abs(gM.y) < ft;
#endif
    if (isFlat) {
        return vec4(rM, 1.0);
    }
    vec2 p0 = (f0 - 0.5) * texel;
    vec2 p3 = (f0 + 2.5) * texel;
    vec3 rT = mwsrS(vec2(p12.x, p0.y));
    vec3 rL = mwsrS(vec2(p0.x, p12.y));
    vec3 rR = mwsrS(vec2(p3.x, p12.y));
    vec3 rB = mwsrS(vec2(p12.x, p3.y));
    vec2 dn = gM * inversesqrt(dot(gM, gM)) * across;
    vec3 ra = mwsrS(uv - dn);
    vec3 rb = mwsrS(uv + dn);
    vec3 K = (w12.x * (w12.y * rM - ku.y * (s.y * rT + t.y * rB)) - w12.y * ku.x * (s.x * rL + t.x * rR)) / (1.0 - ku.x * ku.y);
    vec3 D = K - 0.5 * (ra + rb);
    vec3 R = K + kg * D + kwc * (D - vec3(dot(D, MWSR_LUMA)));
    vec3 mn = min(rM, min(ra, rb));
    vec3 mx = max(rM, max(ra, rb));
    vec3 mg = (mx - mn) * kl;
    return vec4(clamp(R, max(mn - mg, vec3(0.0)), mx + mg), 1.0);
}
