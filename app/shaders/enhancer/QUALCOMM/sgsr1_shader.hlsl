//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

#define SGSR_MOBILE

cbuffer PerFrameConstants : register (b0)
{
    float4 ViewportInfo;
}

// ============================================================================
// VERTEX SHADER OUTPUT STRUCTURE
// ============================================================================

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// ============================================================================
// VERTEX SHADER
// ============================================================================

VS_OUTPUT mainVS(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;
    
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    
    return output;
}

// ============================================================================
// PIXEL SHADER (Upscaler)
// ============================================================================

SamplerState samLinearClamp : register(s0); // Not set from the code, but default LINEAR + CLAMP is fine.
Texture2D<half4> InputTexture : register(t0);
#define  SGSR_H 1

half4 SGSRRH(float2 p)
{
    half4 res = InputTexture.GatherRed(samLinearClamp, p);
    return res;
}
half4 SGSRGH(float2 p)
{
    half4 res = InputTexture.GatherGreen(samLinearClamp, p);
    return res;
}
half4 SGSRBH(float2 p)
{
    half4 res = InputTexture.GatherBlue(samLinearClamp, p);
    return res;
}
half4 SGSRAH(float2 p)
{
    half4 res = InputTexture.GatherAlpha(samLinearClamp, p);
    return res;
}
half4 SGSRRGBH(float2 p)
{
    half4 res = InputTexture.SampleLevel(samLinearClamp, p, 0);
    return res;
}

half4 SGSRH(float2 p, uint channel)
{
    if (channel == 0)
        return SGSRRH(p);
    if (channel == 1)
        return SGSRGH(p);
    if (channel == 2)
        return SGSRBH(p);
    return SGSRAH(p);
}

#include "sgsr1.h"

// =====================================================================================
// 
// SNAPDRAGON GAME SUPER RESOLUTION
// 
// =====================================================================================

half4 SnapdragonGameSuperResolution(float2 uv)
{
	half4 OutColor = half4(0, 0, 0, 1);
    SgsrYuvH(OutColor, uv, ViewportInfo);
    return OutColor;
}

half4 mainPS (float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    return SnapdragonGameSuperResolution(uv);
}
