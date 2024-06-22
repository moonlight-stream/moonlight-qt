#include "d3d11_video_pixel_start.hlsli"

cbuffer CSC_CONST_BUF : register(b1)
{
    min16float3x3 cscMatrix;
    min16float3 offsets;
};

#include "d3d11_video_pixel_end.hlsli"