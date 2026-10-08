// The MIT License(MIT)
//
// Copyright(c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//---------------------------------------------------------------------------------
// NVIDIA Image Scaling SDK  - v1.0.3
//---------------------------------------------------------------------------------
// The NVIDIA Image Scaling SDK provides a single spatial scaling and sharpening algorithm
// for cross-platform support. The scaling algorithm uses a 6-tap scaling filter combined
// with 4 directional scaling and adaptive sharpening filters, which creates nice smooth images
// and sharp edges. In addition, the SDK provides a state-of-the-art adaptive directional sharpening algorithm
// for use in applications where no scaling is required.
//
// The directional scaling and sharpening algorithm is named NVScaler while the adaptive-directional-sharpening-only
// algorithm is named NVSharpen. Both algorithms are provided as compute shaders and
// developers are free to integrate them in their applications. Note that if you integrate NVScaler, you
// should NOT integrate NVSharpen, as NVScaler already includes a sharpening pass
//
// Pipeline Placement
// ------------------
// The call into the NVIDIA Image Scaling shaders must occur during the post-processing phase after tone-mapping.
// Applying the scaling in linear HDR in-game color-space may result in a sharpening effect that is
// either not visible or too strong. Since sharpening algorithms can enhance noisy or grainy regions, it is recommended
// that certain effects such as film grain should occur after NVScaler or NVSharpen. Low-pass filters such as motion blur or
// light bloom are recommended to be applied before NVScaler or NVSharpen to avoid sharpening attenuation.
//
// Color Space and Ranges
// ----------------------
// NVIDIA Image Scaling shaders can process color textures stored as either LDR or HDR with the following
// restrictions:
// 1) LDR
//    - The range of color values must be in the [0, 1] range
//    - The input color texture must be in display-referred color-space after tone mapping and OETF (gamma-correction)
//      has been applied
// 2) HDR PQ
//    - The range of color values must be in the [0, 1] range
//    - The input color texture must be in display-referred color-space after tone mapping with Rec.2020 PQ OETF applied
// 3) HDR Linear
//    - The recommended range of color values is [0, 12.5], where luminance value (as per BT. 709) of
//      1.0 maps to brightness value of 80nits (sRGB peak) and 12.5 maps to 1000nits
//    - The input color texture may have luminance values that are either linear and scene-referred or
//      linear and display-referred (after tone mapping)
//
// If the input color texture sent to NVScaler/NVSharpen is in HDR format set NIS_HDR_MODE define to either
// NIS_HDR_MODE_LINEAR (1) or NIS_HDR_MODE_PQ (2).
//
// Supported Texture Formats
// -------------------------
// Input and output formats:
// Input and output formats are expected to be in the rages defined in previous section and should be
// specified using non-integer data types such as DXGI_FORMAT_R8G8B8A8_UNORM.
//
// Coefficients formats:
// The scaler coefficients and USM coefficients format should be specified using float4 type such as
// DXGI_FORMAT_R32G32B32A32_FLOAT or DXGI_FORMAT_R16G16B16A16_FLOAT.
//
// Resource States, Buffers, and Sampler:
// The game or application calling NVIDIA Image Scaling SDK shaders must ensure that the textures are in
// the correct state.
// - Input color textures must be in pixel shader read state. Shader Resource View (SRV) in DirectX
// - The output texture must be in read/write state. Unordered Access View (UAV) in DirectX
// - The coefficients texture for NVScaler must be in read state. Shader Resource View (SRV) in DirectX
// - The configuration variables must be passed as constant buffer. Constant Buffer View (CBV) in DirectX
// - The sampler for texture pixel sampling. Linear clamp SamplerState in Direct
//
// Adding NVIDIA Image Scaling SDK to a Project
// --------------------------------------------
// Include NIS_Scaler.h directly in your application or alternative use the provided NIS_Main.hlsl shader file.
// Use NIS_Config.h to get the ideal shader dispatch values for your platform, to configure the algorithm constant
// values (NVScalerUpdateConfig, and NVSharpenUpdateConfig), and to access the algorithm coefficients (coef_scale and coef_USM).
//
// Defines:
// NIS_SCALER: default (1) NVScaler, (0) fast NVSharpen only, no upscaling
// NIS_HDR_MODE: default (0) disabled, (1) Linear, (2) PQ
// NIS_BLOCK_WIDTH: pixels per block width. Use GetOptimalBlockWidth query for your platform
// NIS_BLOCK_HEIGHT: pixels per block height. Use GetOptimalBlockHeight query for your platform
// NIS_THREAD_GROUP_SIZE: number of threads per group. Use GetOptimalThreadGroupSize query for your platform
// NIS_USE_HALF_PRECISION: default (0) disabled, (1) enable half pression computation
// NIS_HLSL: (1) enabled, (0) disabled
// NIS_HLSL_6_2: default (0) HLSL v5, (1) HLSL v6.2 forces NIS_HLSL=1
// NIS_GLSL: (1) enabled, (0) disabled
// NIS_VIEWPORT_SUPPORT: default(0) disabled, (1) enable input/output viewport support
// NIS_NV12_SUPPORT: default(0) disabled, (1) enable NV12 input
// NIS_CLAMP_OUTPUT: default(0) disabled, (1) enable output clamp
//
// Default NVScaler shader constants:
// [NIS_BLOCK_WIDTH, NIS_BLOCK_HEIGHT, NIS_THREAD_GROUP_SIZE] = [32, 24, 256]
//
// Default NVSharpen shader constants:
// [NIS_BLOCK_WIDTH, NIS_BLOCK_HEIGHT, NIS_THREAD_GROUP_SIZE] = [32, 32, 256]
// 
// NIS_UNROLL: default [unroll]
// NIS_UNROLL_INNER: default NIS_UNROLL, define in case of a compiler error for inner nested loops
//---------------------------------------------------------------------------------

// NVScaler enable by default. Set to 0 for NVSharpen only
#ifndef NIS_SCALER
#define NIS_SCALER 1
#endif

// HDR Modes
#define NIS_HDR_MODE_NONE  0
#define NIS_HDR_MODE_LINEAR  1
#define NIS_HDR_MODE_PQ  2
#ifndef NIS_HDR_MODE
#define NIS_HDR_MODE NIS_HDR_MODE_NONE
#endif
#define kHDRCompressionFactor  0.282842712f

// Viewport support
#ifndef NIS_VIEWPORT_SUPPORT
#define NIS_VIEWPORT_SUPPORT 0
#endif

// HLSL, GLSL
#if NIS_HLSL==0 && !defined(NIS_GLSL)
#define NIS_GLSL 1
#endif
#if NIS_HLSL_6_2 || (!NIS_GLSL && !NIS_HLSL)
#if defined(NIS_HLSL)
#undef NIS_HLSL
#endif
#define NIS_HLSL 1
#endif
#if NIS_HLSL && NIS_GLSL
#undef NIS_GLSL
#define NIS_GLSL 0
#endif

// Half precision
#ifndef NIS_USE_HALF_PRECISION
#define NIS_USE_HALF_PRECISION 0
#endif

#if NIS_HLSL
// Generic type and function aliases for HLSL
#define NVF float
#define NVF2 float2
#define NVF3 float3
#define NVF4 float4
#define NVI int
#define NVI2 int2
#define NVU uint
#define NVU2 uint2
#define NVB bool
#if NIS_USE_HALF_PRECISION
#if NIS_HLSL_6_2
#define NVH float16_t
#define NVH2 float16_t2
#define NVH3 float16_t3
#define NVH4 float16_t4
#else
#define NVH min16float
#define NVH2 min16float2
#define NVH3 min16float3
#define NVH4 min16float4
#endif // NIS_HLSL_6_2
#else // FP32 types
#define NVH NVF
#define NVH2 NVF2
#define NVH3 NVF3
#define NVH4 NVF4
#endif // NIS_USE_HALF_PRECISION
#define NVSHARED groupshared
#define NVTEX_LOAD(x, pos) x[pos]
#define NVTEX_SAMPLE(x, sampler, pos) x.SampleLevel(sampler, pos, 0)
#define NVTEX_SAMPLE_RED(x, sampler, pos) x.GatherRed(sampler, pos)
#define NVTEX_SAMPLE_GREEN(x, sampler, pos) x.GatherGreen(sampler, pos)
#define NVTEX_SAMPLE_BLUE(x, sampler, pos) x.GatherBlue(sampler, pos)
#define NVTEX_STORE(x, pos, v) x[pos] = v
#ifndef NIS_UNROLL
#define NIS_UNROLL [unroll]
#endif
#endif // NIS_HLSL

// Generic type and function aliases for GLSL
#if NIS_GLSL
#define NVF float
#define NVF2 vec2
#define NVF3 vec3
#define NVF4 vec4
#define NVI int
#define NVI2 ivec2
#define NVU uint
#define NVU2 uvec2
#define NVB bool
#if NIS_USE_HALF_PRECISION
#define NVH float16_t
#define NVH2 f16vec2
#define NVH3 f16vec3
#define NVH4 f16vec4
#else // FP32 types
#define NVH NVF
#define NVH2 NVF2
#define NVH3 NVF3
#define NVH4 NVF4
#endif  // NIS_USE_HALF_PRECISION
#define NVSHARED shared
#define NVTEX_LOAD(x, pos) texelFetch(sampler2D(x, samplerLinearClamp), pos, 0)
#define NVTEX_SAMPLE(x, sampler, pos) textureLod(sampler2D(x, sampler), pos, 0)
#define NVTEX_SAMPLE_RED(x, sampler, pos) textureGather(sampler2D(x, sampler), pos, 0)
#define NVTEX_SAMPLE_GREEN(x, sampler, pos) textureGather(sampler2D(x, sampler), pos, 1)
#define NVTEX_SAMPLE_BLUE(x, sampler, pos) textureGather(sampler2D(x, sampler), pos, 2)
#define NVTEX_STORE(x, pos, v) imageStore(x, NVI2(pos), v)
#define saturate(x) clamp(x, 0, 1)
#define lerp(a, b, x) mix(a, b, x)
#define GroupMemoryBarrierWithGroupSync() groupMemoryBarrier(); barrier()
#ifndef NIS_UNROLL
#define NIS_UNROLL
#endif
#endif // NIS_GLSL

#ifndef NIS_UNROLL_INNER
#define NIS_UNROLL_INNER NIS_UNROLL
#endif

// Texture gather
#ifndef NIS_TEXTURE_GATHER
#define NIS_TEXTURE_GATHER 0
#endif

// NIS Scaling
#define NIS_SCALE_INT 1
#define NIS_SCALE_FLOAT NVF(1.f)

// NIS output clamp
#if NIS_CLAMP_OUTPUT
#if NIS_HDR_MODE == NIS_HDR_MODE_LINEAR
#define NVCLAMP(x) ( clamp(x, 0.0f, 12.5f) )
#else
#define NVCLAMP(x) ( saturate(x) )
#endif
#else
#define NVCLAMP(x) (x)
#endif

NVF getY(NVF3 rgba)
{
#if NIS_HDR_MODE == NIS_HDR_MODE_PQ
    return NVF(0.262f) * rgba.x + NVF(0.678f) * rgba.y + NVF(0.0593f) * rgba.z;
#elif NIS_HDR_MODE == NIS_HDR_MODE_LINEAR
    return sqrt(NVF(0.2126f) * rgba.x + NVF(0.7152f) * rgba.y + NVF(0.0722f) * rgba.z) * kHDRCompressionFactor;
#else
    return NVF(0.2126f) * rgba.x + NVF(0.7152f) * rgba.y + NVF(0.0722f) * rgba.z;
#endif
}

NVF getYLinear(NVF3 rgba)
{
    return NVF(0.2126f) * rgba.x + NVF(0.7152f) * rgba.y + NVF(0.0722f) * rgba.z;
}

NVF3 YUVtoRGB(NVF3 yuv)
{
    float y = yuv.x - 16.0f / 255.0f;
    float u = yuv.y - 128.0f / 255.0f;
    float v = yuv.z - 128.0f / 255.0f;
    NVF3 rgb;
    rgb.x = saturate(1.164f * y + 1.596f * v);
    rgb.y = saturate(1.164f * y - 0.392f * u - 0.813f * v);
    rgb.z = saturate(1.164f * y + 2.017f * u);
    return rgb;
}

#if NIS_SCALER
NVF4 GetEdgeMap(NVF p[4][4], NVI i, NVI j)
#else
NVF4 GetEdgeMap(NVF p[5][5], NVI i, NVI j)
#endif
{
    const NVF g_0 = abs(p[0 + i][0 + j] + p[0 + i][1 + j] + p[0 + i][2 + j] - p[2 + i][0 + j] - p[2 + i][1 + j] - p[2 + i][2 + j]);
    const NVF g_45 = abs(p[1 + i][0 + j] + p[0 + i][0 + j] + p[0 + i][1 + j] - p[2 + i][1 + j] - p[2 + i][2 + j] - p[1 + i][2 + j]);
    const NVF g_90 = abs(p[0 + i][0 + j] + p[1 + i][0 + j] + p[2 + i][0 + j] - p[0 + i][2 + j] - p[1 + i][2 + j] - p[2 + i][2 + j]);
    const NVF g_135 = abs(p[1 + i][0 + j] + p[2 + i][0 + j] + p[2 + i][1 + j] - p[0 + i][1 + j] - p[0 + i][2 + j] - p[1 + i][2 + j]);

    const NVF g_0_90_max = max(g_0, g_90);
    const NVF g_0_90_min = min(g_0, g_90);
    const NVF g_45_135_max = max(g_45, g_135);
    const NVF g_45_135_min = min(g_45, g_135);

    NVF e_0_90 = 0;
    NVF e_45_135 = 0;

    if (g_0_90_max + g_45_135_max == 0)
    {
        return NVF4(0, 0, 0, 0);
    }

    e_0_90 = min(g_0_90_max / (g_0_90_max + g_45_135_max), 1.0f);
    e_45_135 = 1.0f - e_0_90;

    NVB c_0_90 = (g_0_90_max > (g_0_90_min * kDetectRatio)) && (g_0_90_max > kDetectThres) && (g_0_90_max > g_45_135_min);
    NVB c_45_135 = (g_45_135_max > (g_45_135_min * kDetectRatio)) && (g_45_135_max > kDetectThres) && (g_45_135_max > g_0_90_min);
    NVB c_g_0_90 = g_0_90_max == g_0;
    NVB c_g_45_135 = g_45_135_max == g_45;

    NVF f_e_0_90 = (c_0_90 && c_45_135) ? e_0_90 : 1.0f;
    NVF f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0f;

    NVF weight_0 = (c_0_90 && c_g_0_90) ? f_e_0_90 : 0.0f;
    NVF weight_90 = (c_0_90 && !c_g_0_90) ? f_e_0_90 : 0.0f;
    NVF weight_45 = (c_45_135 && c_g_45_135) ? f_e_45_135 : 0.0f;
    NVF weight_135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0f;

    return NVF4(weight_0, weight_90, weight_45, weight_135);
}

#if NIS_SCALER

#ifndef NIS_BLOCK_WIDTH
#define NIS_BLOCK_WIDTH 32
#endif
#ifndef NIS_BLOCK_HEIGHT
#define NIS_BLOCK_HEIGHT 24
#endif
#ifndef NIS_THREAD_GROUP_SIZE
#define NIS_THREAD_GROUP_SIZE 256
#endif
#define kPhaseCount  64
#define kFilterSize  6
#define kSupportSize 6
#define kPadSize     kSupportSize
// 'Tile' is the region of source luminance values that we load into shPixelsY.
// It is the area of source pixels covered by the destination 'Block' plus a
// 3 pixel border of support pixels.
#define kTilePitch              (NIS_BLOCK_WIDTH + kPadSize)
#define kTileSize               (kTilePitch * (NIS_BLOCK_HEIGHT + kPadSize))
// 'EdgeMap' is the region of source pixels for which edge map vectors are derived.
// It is the area of source pixels covered by the destination 'Block' plus a
// 1 pixel border.
#define kEdgeMapPitch           (NIS_BLOCK_WIDTH + 2)
#define kEdgeMapSize            (kEdgeMapPitch * (NIS_BLOCK_HEIGHT + 2))

NVSHARED NVF shPixelsY[kTileSize];
NVSHARED NVH shCoefScaler[kPhaseCount][kFilterSize];
NVSHARED NVH shCoefUSM[kPhaseCount][kFilterSize];
NVSHARED NVH4 shEdgeMap[kEdgeMapSize];

void LoadFilterBanksSh(NVI i0) {
    // Load up filter banks to shared memory
    // The work is spread over (kPhaseCount * 2) threads
    NVI i = i0;
#if( kPhaseCount * 2 > NIS_THREAD_GROUP_SIZE )
    for (; i < kPhaseCount * 2; i += NIS_THREAD_GROUP_SIZE)
#else
    if (i < kPhaseCount * 2)
#endif
    {
        NVI phase = i >> 1;
        NVI vIdx = i & 1;

        NVH4 v = NVH4(NVTEX_LOAD(coef_scaler, NVI2(vIdx, phase)));
        NVI filterOffset = vIdx * 4;
        shCoefScaler[phase][filterOffset + 0] = v.x;
        shCoefScaler[phase][filterOffset + 1] = v.y;
        if (vIdx == 0)
        {
            shCoefScaler[phase][2] = v.z;
            shCoefScaler[phase][3] = v.w;
        }

        v = NVH4(NVTEX_LOAD(coef_usm, NVI2(vIdx, phase)));
        shCoefUSM[phase][filterOffset + 0] = v.x;
        shCoefUSM[phase][filterOffset + 1] = v.y;
        if (vIdx == 0)
        {
            shCoefUSM[phase][2] = v.z;
            shCoefUSM[phase][3] = v.w;
        }
    }
}


NVF CalcLTI(NVF p0, NVF p1, NVF p2, NVF p3, NVF p4, NVF p5, NVI phase_index)
{
    const NVB selector = (phase_index <= kPhaseCount / 2);
    NVF sel = selector ? p0 : p3;
    const NVF a_min = min(min(p1, p2), sel);
    const NVF a_max = max(max(p1, p2), sel);
    sel = selector ? p2 : p5;
    const NVF b_min = min(min(p3, p4), sel);
    const NVF b_max = max(max(p3, p4), sel);

    const NVF a_cont = a_max - a_min;
    const NVF b_cont = b_max - b_min;

    const NVF cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + kEps);
    return (1.0f - saturate((cont_ratio - kMinContrastRatio) * kRatioNorm)) * kContrastBoost;
}

NVF4 GetInterpEdgeMap(const NVF4 edge[2][2], NVF phase_frac_x, NVF phase_frac_y)
{
    NVF4 h0 = lerp(edge[0][0], edge[0][1], phase_frac_x);
    NVF4 h1 = lerp(edge[1][0], edge[1][1], phase_frac_x);
    return lerp(h0, h1, phase_frac_y);
}

NVF EvalPoly6(const NVF pxl[6], NVI phase_int)
{
    NVF y = 0.f;
    {
        NIS_UNROLL
        for (NVI i = 0; i < 6; ++i)
        {
            y += shCoefScaler[phase_int][i] * pxl[i];
        }
    }
    NVF y_usm = 0.f;
    {
        NIS_UNROLL
        for (NVI i = 0; i < 6; ++i)
        {
            y_usm += shCoefUSM[phase_int][i] * pxl[i];
        }
    }

    // let's compute a piece-wise ramp based on luma
    const NVF y_scale = 1.0f - saturate((y * (1.0f / NIS_SCALE_FLOAT) - kSharpStartY) * kSharpScaleY);

    // scale the ramp to sharpen as a function of luma
    const NVF y_sharpness = y_scale * kSharpStrengthScale + kSharpStrengthMin;

    y_usm *= y_sharpness;

    // scale the ramp to limit USM as a function of luma
    const NVF y_sharpness_limit = (y_scale * kSharpLimitScale + kSharpLimitMin) * y;

    y_usm = min(y_sharpness_limit, max(-y_sharpness_limit, y_usm));
    // reduce ringing
    y_usm *= CalcLTI(pxl[0], pxl[1], pxl[2], pxl[3], pxl[4], pxl[5], phase_int);

    return y + y_usm;
}

NVF FilterNormal(const NVF p[6][6], NVI phase_x_frac_int, NVI phase_y_frac_int)
{
    NVF h_acc = 0.0f;
    NIS_UNROLL
    for (NVI j = 0; j < 6; ++j)
    {
        NVF v_acc = 0.0f;
        NIS_UNROLL
        for (NVI i = 0; i < 6; ++i)
        {
            v_acc += p[i][j] * shCoefScaler[phase_y_frac_int][i];
        }
        h_acc += v_acc * shCoefScaler[phase_x_frac_int][j];
    }

    // let's return the sum unpacked -> we can accumulate it later
    return h_acc;
}

NVF AddDirFilters(NVF p[6][6], NVF phase_x_frac, NVF phase_y_frac, NVI phase_x_frac_int, NVI phase_y_frac_int, NVF4 w)
{
    NVF f = 0;
    if (w.x > 0.0f)
    {
        // 0 deg filter
        NVF interp0Deg[6];
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; ++i)
                {
                    interp0Deg[i] = lerp(p[i][2], p[i][3], phase_x_frac);
                }
        }
        f += EvalPoly6(interp0Deg, phase_y_frac_int) * w.x;
    }
    if (w.y > 0.0f)
    {
        // 90 deg filter
        NVF interp90Deg[6];
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; ++i)
                {
                    interp90Deg[i] = lerp(p[2][i], p[3][i], phase_y_frac);
                }
        }

        f += EvalPoly6(interp90Deg, phase_x_frac_int) * w.y;
    }
    if (w.z > 0.0f)
    {
        //45 deg filter
        NVF pphase_b45 = 0.5f + 0.5f * (phase_x_frac - phase_y_frac);

        NVF temp_interp45Deg[7];
        temp_interp45Deg[1] = lerp(p[2][1], p[1][2], pphase_b45);
        temp_interp45Deg[3] = lerp(p[3][2], p[2][3], pphase_b45);
        temp_interp45Deg[5] = lerp(p[4][3], p[3][4], pphase_b45);
        {
            pphase_b45 = pphase_b45 - 0.5f;
            NVF a = (pphase_b45 >= 0.f) ? p[0][2] : p[2][0];
            NVF b = (pphase_b45 >= 0.f) ? p[1][3] : p[3][1];
            NVF c = (pphase_b45 >= 0.f) ? p[2][4] : p[4][2];
            NVF d = (pphase_b45 >= 0.f) ? p[3][5] : p[5][3];
            temp_interp45Deg[0] = lerp(p[1][1], a, abs(pphase_b45));
            temp_interp45Deg[2] = lerp(p[2][2], b, abs(pphase_b45));
            temp_interp45Deg[4] = lerp(p[3][3], c, abs(pphase_b45));
            temp_interp45Deg[6] = lerp(p[4][4], d, abs(pphase_b45));
        }

        NVF interp45Deg[6];
        NVF pphase_p45 = phase_x_frac + phase_y_frac;
        if (pphase_p45 >= 1)
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; i++)
                {
                    interp45Deg[i] = temp_interp45Deg[i + 1];
                }
            pphase_p45 = pphase_p45 - 1;
        }
        else
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; i++)
                {
                    interp45Deg[i] = temp_interp45Deg[i];
                }
        }

        f += EvalPoly6(interp45Deg, NVI(pphase_p45 * 64)) * w.z;
    }
    if (w.w > 0.0f)
    {
        //135 deg filter
        NVF pphase_b135 = 0.5f * (phase_x_frac + phase_y_frac);

        NVF temp_interp135Deg[7];
        temp_interp135Deg[1] = lerp(p[3][1], p[4][2], pphase_b135);
        temp_interp135Deg[3] = lerp(p[2][2], p[3][3], pphase_b135);
        temp_interp135Deg[5] = lerp(p[1][3], p[2][4], pphase_b135);
        {
            pphase_b135 = pphase_b135 - 0.5f;
            NVF a = (pphase_b135 >= 0.f) ? p[5][2] : p[3][0];
            NVF b = (pphase_b135 >= 0.f) ? p[4][3] : p[2][1];
            NVF c = (pphase_b135 >= 0.f) ? p[3][4] : p[1][2];
            NVF d = (pphase_b135 >= 0.f) ? p[2][5] : p[0][3];
            temp_interp135Deg[0] = lerp(p[4][1], a, abs(pphase_b135));
            temp_interp135Deg[2] = lerp(p[3][2], b, abs(pphase_b135));
            temp_interp135Deg[4] = lerp(p[2][3], c, abs(pphase_b135));
            temp_interp135Deg[6] = lerp(p[1][4], d, abs(pphase_b135));
        }

        NVF interp135Deg[6];
        NVF pphase_p135 = 1 + (phase_x_frac - phase_y_frac);
        if (pphase_p135 >= 1)
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; ++i)
                {
                    interp135Deg[i] = temp_interp135Deg[i + 1];
                }
            pphase_p135 = pphase_p135 - 1;
        }
        else
        {
            NIS_UNROLL
                for (NVI i = 0; i < 6; ++i)
                {
                    interp135Deg[i] = temp_interp135Deg[i];
                }
        }

        f += EvalPoly6(interp135Deg, NVI(pphase_p135 * 64)) * w.w;
    }
    return f;
}


//-----------------------------------------------------------------------------------------------
// NVScaler
//-----------------------------------------------------------------------------------------------
void NVScaler(NVU2 blockIdx, NVU threadIdx)
{
    // Figure out the range of pixels from input image that would be needed to be loaded for this thread-block
    NVI dstBlockX = NVI(NIS_BLOCK_WIDTH * blockIdx.x);
    NVI dstBlockY = NVI(NIS_BLOCK_HEIGHT * blockIdx.y);

    const NVI srcBlockStartX = NVI(floor((dstBlockX + 0.5f) * kScaleX - 0.5f));
    const NVI srcBlockStartY = NVI(floor((dstBlockY + 0.5f) * kScaleY - 0.5f));
    const NVI srcBlockEndX = NVI(ceil((dstBlockX + NIS_BLOCK_WIDTH + 0.5f) * kScaleX - 0.5f));
    const NVI srcBlockEndY = NVI(ceil((dstBlockY + NIS_BLOCK_HEIGHT + 0.5f) * kScaleY - 0.5f));

    NVI numTilePixelsX = srcBlockEndX - srcBlockStartX + kSupportSize - 1;
    NVI numTilePixelsY = srcBlockEndY - srcBlockStartY + kSupportSize - 1;

    // round-up load region to even size since we're loading in 2x2 batches
    numTilePixelsX += numTilePixelsX & 0x1;
    numTilePixelsY += numTilePixelsY & 0x1;
    const NVI numTilePixels = numTilePixelsX * numTilePixelsY;

    // calculate the equivalent values for the edge map
    const NVI numEdgeMapPixelsX = numTilePixelsX - kSupportSize + 2;
    const NVI numEdgeMapPixelsY = numTilePixelsY - kSupportSize + 2;
    const NVI numEdgeMapPixels = numEdgeMapPixelsX * numEdgeMapPixelsY;

    // fill in input luma tile (shPixelsY) in batches of 2x2 pixels
    // we use texture gather to get extra support necessary
    // to compute 2x2 edge map outputs too
    {
        for (NVU i = threadIdx * 2; i < NVU(numTilePixels) >> 1; i += NIS_THREAD_GROUP_SIZE * 2)
        {
            NVU py = (i / numTilePixelsX) * 2;
            NVU px = i % numTilePixelsX;

            // 0.5 to be in the center of texel
            // - (kSupportSize - 1) / 2 to shift by the kernel support size
            NVF kShift = 0.5f - (kSupportSize - 1) / 2;
#if NIS_VIEWPORT_SUPPORT
            const NVF tx = (srcBlockStartX + px + kInputViewportOriginX + kShift) * kSrcNormX;
            const NVF ty = (srcBlockStartY + py + kInputViewportOriginY + kShift) * kSrcNormY;
#else
            const NVF tx = (srcBlockStartX + px + kShift) * kSrcNormX;
            const NVF ty = (srcBlockStartY + py + kShift) * kSrcNormY;
#endif
            NVF p[2][2];
#if NIS_TEXTURE_GATHER
            {
                const NVF4 sr = NVTEX_SAMPLE_RED(in_texture, samplerLinearClamp, NVF2(tx, ty));
                const NVF4 sg = NVTEX_SAMPLE_GREEN(in_texture, samplerLinearClamp, NVF2(tx, ty));
                const NVF4 sb = NVTEX_SAMPLE_BLUE(in_texture, samplerLinearClamp, NVF2(tx, ty));

                p[0][0] = getY(NVF3(sr.w, sg.w, sb.w));
                p[0][1] = getY(NVF3(sr.z, sg.z, sb.z));
                p[1][0] = getY(NVF3(sr.x, sg.x, sb.x));
                p[1][1] = getY(NVF3(sr.y, sg.y, sb.y));
            }
#else
            NIS_UNROLL_INNER
            for (NVI j = 0; j < 2; j++)
            {
                NIS_UNROLL_INNER
                for (NVI k = 0; k < 2; k++)
                {
#if NIS_NV12_SUPPORT
                    p[j][k] = NVTEX_SAMPLE(in_texture_y, samplerLinearClamp, NVF2(tx + k * kSrcNormX, ty + j * kSrcNormY));
#else
                    const NVF4 px = NVTEX_SAMPLE(in_texture, samplerLinearClamp, NVF2(tx + k * kSrcNormX, ty + j * kSrcNormY));
                    p[j][k] = getY(px.xyz);
#endif
                }
            }
#endif
            const NVU idx = py * kTilePitch + px;
            shPixelsY[idx] = NVH(p[0][0]);
            shPixelsY[idx + 1] = NVH(p[0][1]);
            shPixelsY[idx + kTilePitch] = NVH(p[1][0]);
            shPixelsY[idx + kTilePitch + 1] = NVH(p[1][1]);
        }
    }
    GroupMemoryBarrierWithGroupSync();
    {
        // fill in the edge map of 2x2 pixels
        for (NVU i = threadIdx * 2; i < NVU(numEdgeMapPixels) >> 1; i += NIS_THREAD_GROUP_SIZE * 2)
        {
            NVU py = (i / numEdgeMapPixelsX) * 2;
            NVU px = i % numEdgeMapPixelsX;

            const NVU edgeMapIdx = py * kEdgeMapPitch + px;

            NVU tileCornerIdx = (py + 1) * kTilePitch + px + 1;
            NVF p[4][4];
            NIS_UNROLL_INNER
            for (NVI j = 0; j < 4; j++)
            {
                NIS_UNROLL_INNER
                for (NVI k = 0; k < 4; k++)
                {
                    p[j][k] = shPixelsY[tileCornerIdx + j * kTilePitch + k];
                }
            }

            shEdgeMap[edgeMapIdx] = NVH4(GetEdgeMap(p, 0, 0));
            shEdgeMap[edgeMapIdx + 1] = NVH4(GetEdgeMap(p, 0, 1));
            shEdgeMap[edgeMapIdx + kEdgeMapPitch] = NVH4(GetEdgeMap(p, 1, 0));
            shEdgeMap[edgeMapIdx + kEdgeMapPitch + 1] = NVH4(GetEdgeMap(p, 1, 1));
        }
    }
    LoadFilterBanksSh(NVI(threadIdx));
    GroupMemoryBarrierWithGroupSync();

    // output coord within a tile
    const NVI2 pos = NVI2(NVU(threadIdx) % NVU(NIS_BLOCK_WIDTH), NVU(threadIdx) / NVU(NIS_BLOCK_WIDTH));
    // x coord inside the output image
    const NVI dstX = dstBlockX + pos.x;
    // x coord inside the input image
    const NVF srcX = (0.5f + dstX) * kScaleX - 0.5f;
    // nearest integer part
    const NVI px = NVI(floor(srcX) - srcBlockStartX);
    // fractional part
    const NVF fx = srcX - floor(srcX);
    // discretized phase
    const NVI fx_int = NVI(fx * kPhaseCount);
#if NIS_VIEWPORT_SUPPORT
    if (NVU(srcX) > kInputViewportWidth || NVU(dstX) > kOutputViewportWidth)
    {
        return;
    }
#endif
    for (NVI k = 0; k < NIS_BLOCK_WIDTH * NIS_BLOCK_HEIGHT / NIS_THREAD_GROUP_SIZE; ++k)
    {
        // y coord inside the output image
        const NVI dstY = dstBlockY + pos.y + k * (NIS_THREAD_GROUP_SIZE / NIS_BLOCK_WIDTH);
        // y coord inside the input image
        const NVF srcY = (0.5f + dstY) * kScaleY - 0.5f;
#if NIS_VIEWPORT_SUPPORT
        if (!(NVU(srcY) > kInputViewportHeight || NVU(dstY) > kOutputViewportHeight))
#endif
        {
            // nearest integer part
            const NVI py = NVI(floor(srcY) - srcBlockStartY);
            // fractional part
            const NVF fy = srcY - floor(srcY);
            // discretized phase
            const NVI fy_int = NVI(fy * kPhaseCount);

            // generate weights for directional filters
            const NVI startEdgeMapIdx = py * kEdgeMapPitch + px;
            NVF4 edge[2][2];
            NIS_UNROLL
            for (NVI i = 0; i < 2; i++)
            {
                NIS_UNROLL
                for (NVI j = 0; j < 2; j++)
                {
                    // need to shift edge map sampling since it's a 2x2 centered inside 6x6 grid
                    edge[i][j] = shEdgeMap[startEdgeMapIdx + (i * kEdgeMapPitch) + j];
                }
            }
            const NVF4 w = GetInterpEdgeMap(edge, fx, fy) * NIS_SCALE_INT;

            // load 6x6 support to regs
            const NVI startTileIdx = py * kTilePitch + px;
            NVF p[6][6];
            {
                NIS_UNROLL
                for (NVI i = 0; i < 6; ++i)
                {
                    NIS_UNROLL
                    for (NVI j = 0; j < 6; ++j)
                    {
                        p[i][j] = shPixelsY[startTileIdx + i * kTilePitch + j];
                    }
                }
            }

            // weigth for luma
            const NVF baseWeight = NIS_SCALE_FLOAT - w.x - w.y - w.z - w.w;

            // final luma is a weighted product of directional & normal filters
            NVF opY = 0;

            // get traditional scaler filter output
            opY += FilterNormal(p, fx_int, fy_int) * baseWeight;

            // get directional filter bank output
            opY += AddDirFilters(p, fx, fy, fx_int, fy_int, w);

#if NIS_VIEWPORT_SUPPORT
            NVF2 coord = NVF2((srcX + kInputViewportOriginX + 0.5f) * kSrcNormX, (srcY + kInputViewportOriginY + 0.5f) * kSrcNormY);
            NVF2 dstCoord = NVF2(dstX + kOutputViewportOriginX, dstY + kOutputViewportOriginY);
#else
            NVF2 coord = NVF2((srcX + 0.5f) * kSrcNormX, (srcY + 0.5f) * kSrcNormY);
            NVF2 dstCoord = NVF2(dstX, dstY);
#endif
            // do bilinear tap for chroma upscaling
#if NIS_NV12_SUPPORT
            NVF y = NVTEX_SAMPLE(in_texture_y, samplerLinearClamp, coord);
            NVF2 uv = NVTEX_SAMPLE(in_texture_uv, samplerLinearClamp, coord);
            NVF4 op = NVF4(YUVtoRGB(NVF3(y, uv)), 1.0f);
#else
            NVF4 op = NVTEX_SAMPLE(in_texture, samplerLinearClamp, coord);
            NVF y = getY(NVF3(op.x, op.y, op.z));
#endif

#if NIS_HDR_MODE == NIS_HDR_MODE_LINEAR
            const NVF kEps = 1e-4f;
            const NVF kNorm = 1.0f / (NIS_SCALE_FLOAT * kHDRCompressionFactor);
            const NVF opYN = max(opY, 0.0f) * kNorm;
            const NVF corr = (opYN * opYN + kEps) / (max(getYLinear(NVF3(op.x, op.y, op.z)), 0.0f) + kEps);
            op.x *= corr;
            op.y *= corr;
            op.z *= corr;
#else
            const NVF corr = opY * (1.0f / NIS_SCALE_FLOAT) - y;
            op.x += corr;
            op.y += corr;
            op.z += corr;
#endif
            NVTEX_STORE(out_texture, dstCoord, NVCLAMP(op));
        }
    }
}
#else

#ifndef NIS_BLOCK_WIDTH
#define NIS_BLOCK_WIDTH 32
#endif
#ifndef NIS_BLOCK_HEIGHT
#define NIS_BLOCK_HEIGHT 32
#endif
#ifndef NIS_THREAD_GROUP_SIZE
#define NIS_THREAD_GROUP_SIZE 256
#endif

#define kSupportSize 5
#define kNumPixelsX  (NIS_BLOCK_WIDTH + kSupportSize + 1)
#define kNumPixelsY  (NIS_BLOCK_HEIGHT + kSupportSize + 1)

NVSHARED NVF shPixelsY[kNumPixelsY][kNumPixelsX];

NVF CalcLTIFast(const NVF y[5])
{
    const NVF a_min = min(min(y[0], y[1]), y[2]);
    const NVF a_max = max(max(y[0], y[1]), y[2]);

    const NVF b_min = min(min(y[2], y[3]), y[4]);
    const NVF b_max = max(max(y[2], y[3]), y[4]);

    const NVF a_cont = a_max - a_min;
    const NVF b_cont = b_max - b_min;

    const NVF cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + kEps);
    return (1.0f - saturate((cont_ratio - kMinContrastRatio) * kRatioNorm)) * kContrastBoost;
}

NVF EvalUSM(const NVF pxl[5], const NVF sharpnessStrength, const NVF sharpnessLimit)
{
    // USM profile
    NVF y_usm = -0.6001f * pxl[1] + 1.2002f * pxl[2] - 0.6001f * pxl[3];
    // boost USM profile
    y_usm *= sharpnessStrength;
    // clamp to the limit
    y_usm = min(sharpnessLimit, max(-sharpnessLimit, y_usm));
    // reduce ringing
    y_usm *= CalcLTIFast(pxl);

    return y_usm;
}

NVF4 GetDirUSM(const NVF p[5][5])
{
    // sharpness boost & limit are the same for all directions
    const NVF scaleY = 1.0f - saturate((p[2][2] - kSharpStartY) * kSharpScaleY);
    // scale the ramp to sharpen as a function of luma
    const NVF sharpnessStrength = scaleY * kSharpStrengthScale + kSharpStrengthMin;
    // scale the ramp to limit USM as a function of luma
    const NVF sharpnessLimit = (scaleY * kSharpLimitScale + kSharpLimitMin) * p[2][2];

    NVF4 rval;
    // 0 deg filter
    NVF interp0Deg[5];
    {
        for (NVI i = 0; i < 5; ++i)
        {
            interp0Deg[i] = p[i][2];
        }
    }

    rval.x = EvalUSM(interp0Deg, sharpnessStrength, sharpnessLimit);

    // 90 deg filter
    NVF interp90Deg[5];
    {
        for (NVI i = 0; i < 5; ++i)
        {
            interp90Deg[i] = p[2][i];
        }
    }

    rval.y = EvalUSM(interp90Deg, sharpnessStrength, sharpnessLimit);

    //45 deg filter
    NVF interp45Deg[5];
    interp45Deg[0] = p[1][1];
    interp45Deg[1] = lerp(p[2][1], p[1][2], 0.5f);
    interp45Deg[2] = p[2][2];
    interp45Deg[3] = lerp(p[3][2], p[2][3], 0.5f);
    interp45Deg[4] = p[3][3];

    rval.z = EvalUSM(interp45Deg, sharpnessStrength, sharpnessLimit);

    //135 deg filter
    NVF interp135Deg[5];
    interp135Deg[0] = p[3][1];
    interp135Deg[1] = lerp(p[3][2], p[2][1], 0.5f);
    interp135Deg[2] = p[2][2];
    interp135Deg[3] = lerp(p[2][3], p[1][2], 0.5f);
    interp135Deg[4] = p[1][3];

    rval.w = EvalUSM(interp135Deg, sharpnessStrength, sharpnessLimit);
    return rval;
}

//-----------------------------------------------------------------------------------------------
// NVSharpen
//-----------------------------------------------------------------------------------------------
void NVSharpen(NVU2 blockIdx, NVU threadIdx)
{
    const NVI dstBlockX = NVI(NIS_BLOCK_WIDTH * blockIdx.x);
    const NVI dstBlockY = NVI(NIS_BLOCK_HEIGHT * blockIdx.y);

    // fill in input luma tile in batches of 2x2 pixels
    // we use texture gather to get extra support necessary
    // to compute 2x2 edge map outputs too
    const NVF kShift = 0.5f - kSupportSize / 2;

    for (NVI i = NVI(threadIdx) * 2; i < kNumPixelsX * kNumPixelsY / 2; i += NIS_THREAD_GROUP_SIZE * 2)
    {
        NVU2 pos = NVU2(NVU(i) % NVU(kNumPixelsX), NVU(i) / NVU(kNumPixelsX) * 2);
        NIS_UNROLL
        for (NVI dy = 0; dy < 2; dy++)
        {
            NIS_UNROLL
            for (NVI dx = 0; dx < 2; dx++)
            {
#if NIS_VIEWPORT_SUPPORT
                const NVF tx = (dstBlockX + pos.x + kInputViewportOriginX + dx + kShift) * kSrcNormX;
                const NVF ty = (dstBlockY + pos.y + kInputViewportOriginY + dy + kShift) * kSrcNormY;
#else
                const NVF tx = (dstBlockX + pos.x + dx + kShift) * kSrcNormX;
                const NVF ty = (dstBlockY + pos.y + dy + kShift) * kSrcNormY;
#endif
#if NIS_NV12_SUPPORT
                shPixelsY[pos.y + dy][pos.x + dx] = NVTEX_SAMPLE(in_texture_y, samplerLinearClamp, NVF2(tx, ty));
#else
                const NVF4 px = NVTEX_SAMPLE(in_texture, samplerLinearClamp, NVF2(tx, ty));
                shPixelsY[pos.y + dy][pos.x + dx] = getY(px.xyz);
#endif
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    for (NVI k = NVI(threadIdx); k < NIS_BLOCK_WIDTH * NIS_BLOCK_HEIGHT; k += NIS_THREAD_GROUP_SIZE)
    {
        const NVI2 pos = NVI2(NVU(k) % NVU(NIS_BLOCK_WIDTH), NVU(k) / NVU(NIS_BLOCK_WIDTH));

        // load 5x5 support to regs
        NVF p[5][5];
        NIS_UNROLL
        for (NVI i = 0; i < 5; ++i)
        {
            NIS_UNROLL
            for (NVI j = 0; j < 5; ++j)
            {
                p[i][j] = shPixelsY[pos.y + i][pos.x + j];
            }
        }

        // get directional filter bank output
        NVF4 dirUSM = GetDirUSM(p);

        // generate weights for directional filters
        NVF4 w = GetEdgeMap(p, kSupportSize / 2 - 1, kSupportSize / 2 - 1);

        // final USM is a weighted sum filter outputs
        const NVF usmY = (dirUSM.x * w.x + dirUSM.y * w.y + dirUSM.z * w.z + dirUSM.w * w.w);

        // do bilinear tap and correct rgb texel so it produces new sharpened luma
        const NVI dstX = dstBlockX + pos.x;
        const NVI dstY = dstBlockY + pos.y;

#if NIS_VIEWPORT_SUPPORT
        NVF2 coord = NVF2((dstX + kInputViewportOriginX + 0.5f) * kSrcNormX, (dstY + kInputViewportOriginY + 0.5f) * kSrcNormY);
        NVF2 dstCoord = NVF2(dstX + kOutputViewportOriginX, dstY + kOutputViewportOriginY);
        if (!(NVU(dstX) > kOutputViewportWidth || NVU(dstY) > kOutputViewportHeight))
#else
        NVF2 coord = NVF2((dstX + 0.5f) * kSrcNormX, (dstY + 0.5f) * kSrcNormY);
        NVF2 dstCoord = NVF2(dstX, dstY);
#endif
        {
#if NIS_NV12_SUPPORT
            NVF y = NVTEX_SAMPLE(in_texture_y, samplerLinearClamp, coord);
            NVF2 uv = NVTEX_SAMPLE(in_texture_uv, samplerLinearClamp, coord);
            NVF4 op = NVF4(YUVtoRGB(NVF3(y, uv)), 1.0f);
#else
            NVF4 op = NVTEX_SAMPLE(in_texture, samplerLinearClamp, coord);
#endif
#if NIS_HDR_MODE == NIS_HDR_MODE_LINEAR
            const NVF kEps = 1e-4f * kHDRCompressionFactor * kHDRCompressionFactor;
            NVF newY = p[2][2] + usmY;
            newY = max(newY, 0.0f);
            const NVF oldY = p[2][2];
            const NVF corr = (newY * newY + kEps) / (oldY * oldY + kEps);
            op.x *= corr;
            op.y *= corr;
            op.z *= corr;
#else
            op.x += usmY;
            op.y += usmY;
            op.z += usmY;
#endif
            NVTEX_STORE(out_texture, dstCoord, NVCLAMP(op));
        }
    }
}
#endif