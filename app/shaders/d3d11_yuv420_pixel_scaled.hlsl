// YUV420 -> RGB with a high-quality resampling kernel folded into the same pass.
//
// Moonlight's stock path binds one sampler created with
// D3D11_FILTER_MIN_MAG_MIP_LINEAR (d3d11va.cpp:1615) and every pixel shader uses
// it, so the 1440p -> 4K step is bilinear. Bilinear costs about 1 dB against
// bicubic once the image is filtered to the viewer's acuity limit, and that is
// the one real quality win available in this pipeline.
//
// Sampling higher-order kernels inside this pass (rather than adding two
// separable passes and intermediate render targets) keeps the change small: no
// new textures, no viewport juggling, no extra vertex buffer. The cost is up to
// 36 fetches per plane, which stays comfortably inside a modern GPU's fill rate.
//
// Two variants compile from this file:
//   fxc ... /DUSE_LANCZOS3=1   -> 6-tap Lanczos (a = 3)
//   fxc ... (no define)        -> 4-tap Catmull-Rom bicubic (a = -0.5)

Texture2D<min16float> luminancePlane : register(t0);
Texture2D<min16float2> chrominancePlane : register(t1);
SamplerState theSampler : register(s0);

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

cbuffer CSC_CONST_BUF : register(b0)
{
    min16float3x3 cscMatrix;
    min16float3 offsets;
    min16float2 chromaOffset;
    min16float2 chromaTexMax;
};

#ifdef USE_LANCZOS3
static const int TAPS_BEFORE = 2;   // center-2 .. center+3
static const int TAPS_AFTER = 3;
#else
static const int TAPS_BEFORE = 1;   // center-1 .. center+2
static const int TAPS_AFTER = 2;
#endif

float kernel_weight(float x)
{
    x = abs(x);
#ifdef USE_LANCZOS3
    const float a = 3.0;
    if (x < 1e-6) return 1.0;
    if (x >= a) return 0.0;
    float px = 3.14159265359 * x;
    return (a * sin(px) * sin(px / a)) / (px * px);
#else
    // Catmull-Rom (a = -0.5)
    if (x < 1.0) return ((1.5 * x - 2.5) * x) * x + 1.0;
    if (x < 2.0) return (((-0.5 * x + 2.5) * x) - 4.0) * x + 2.0;
    return 0.0;
#endif
}

min16float3 fetch_yuv(float2 uv, float2 lumaTexel, float2 lumaDims)
{
    // Clamp to texel centres so taps never read the alignment padding row
    float2 cuv = clamp(uv, lumaTexel * 0.5, 1.0 - lumaTexel * 0.5);
    min16float y = luminancePlane.Sample(theSampler, cuv);
    // Chroma shares the tap position; at half resolution its own filtering is
    // dominated by the 4:2:0 upsampling anyway, so no extra tap shift is needed.
    min16float2 c = chrominancePlane.Sample(theSampler, min(cuv + chromaOffset, chromaTexMax.rg));
    return min16float3(y, c);
}

min16float4 main(ShaderInput input) : SV_TARGET
{
    uint lw, lh;
    luminancePlane.GetDimensions(lw, lh);
    float2 lumaDims = float2(lw, lh);
    float2 lumaTexel = 1.0 / lumaDims;

    float2 src = input.tex * lumaDims - 0.5;
    float2 base = floor(src);
    float2 frac2 = src - base;

    min16float3 acc = 0.0;
    float wsum = 0.0;
    [unroll]
    for (int j = -TAPS_BEFORE; j <= TAPS_AFTER; j++) {
        float wy = kernel_weight(float(j) - frac2.y);
        [unroll]
        for (int i = -TAPS_BEFORE; i <= TAPS_AFTER; i++) {
            float wx = kernel_weight(float(i) - frac2.x);
            float w = wx * wy;
            if (w == 0.0) {
                continue;
            }
            float2 tap = (base + float2(i, j) + 0.5) * lumaTexel;
            acc += fetch_yuv(tap, lumaTexel, lumaDims) * w;
            wsum += w;
        }
    }

    min16float3 yuv = (wsum > 1e-6) ? acc / wsum : fetch_yuv(input.tex, lumaTexel, lumaDims);

    yuv -= offsets;
    yuv = mul(yuv, cscMatrix);
    return min16float4(yuv, 1.0);
}
