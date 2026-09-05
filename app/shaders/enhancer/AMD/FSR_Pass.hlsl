// FidelityFX Super Resolution Sample
//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
cbuffer cb : register(b0)
{
    uint4 Const0;
    uint4 Const1;
    uint4 Const2;
    uint4 Const3;
};

#define A_GPU 1
#define A_HLSL 1
#define A_HLSL_6_2 1

SamplerState		samLinearClamp : register(s0);

#if ADVANCED_SHADER
    #define A_HALF
    #include "ffx_a.h"
    Texture2D<AH4> InputTexture : register(t0);
    RWTexture2D<AH4> OutputTexture : register(u0);
    #if APPLY_EASU
        #define FSR_EASU_H 1
        AH4 FsrEasuRH(AF2 p) { AH4 res = InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); return res; }
        AH4 FsrEasuGH(AF2 p) { AH4 res = InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); return res; }
        AH4 FsrEasuBH(AF2 p) { AH4 res = InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); return res; }	
    #endif
    #if APPLY_RCAS
        #define FSR_RCAS_H
        AH4 FsrRcasLoadH(ASW2 p) { return InputTexture.Load(ASW3(ASW2(p), 0)); }
        void FsrRcasInputH(inout AH1 r,inout AH1 g,inout AH1 b){}
    #endif
#else
    #include "ffx_a.h"
    Texture2D InputTexture : register(t0);
    RWTexture2D<float4> OutputTexture : register(u0);
    #if APPLY_EASU
        #define FSR_EASU_F 1
        AF4 FsrEasuRF(AF2 p) { AF4 res = InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); return res; }
        AF4 FsrEasuGF(AF2 p) { AF4 res = InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); return res; }
        AF4 FsrEasuBF(AF2 p) { AF4 res = InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); return res; }
    #endif
    #if APPLY_RCAS
        #define FSR_RCAS_F
        AF4 FsrRcasLoadF(ASU2 p) { return InputTexture.Load(int3(ASU2(p), 0)); }
        void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
    #endif
#endif

#include "ffx_fsr1.h"

void CurrFilter(int2 pos)
{
#if APPLY_EASU
    #if ADVANCED_SHADER
        AH3 c;
        FsrEasuH(c, pos, Const0, Const1, Const2, Const3);
        OutputTexture[pos] = AH4(c, 1);
    #else
        AF3 c;
        FsrEasuF(c, pos, Const0, Const1, Const2, Const3);
        OutputTexture[pos] = float4(c, 1);
    #endif
#endif
#if APPLY_RCAS
    #if ADVANCED_SHADER
        AH3 c;
        FsrRcasH(c.r, c.g, c.b, pos, Const0);
        OutputTexture[pos] = AH4(c, 1);
    #else
        AF3 c;
        FsrRcasF(c.r, c.g, c.b, pos, Const0);
        OutputTexture[pos] = float4(c, 1);
    #endif
#endif
}

[numthreads(64, 1, 1)]
void mainCS(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
{
    // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    AU2 gxy = ARmp8x8(LocalThreadId.x) + AU2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
    CurrFilter(gxy);
    gxy.x += 8u;
    CurrFilter(gxy);
    gxy.y += 8u;
    CurrFilter(gxy);
    gxy.x -= 8u;
    CurrFilter(gxy);
}

