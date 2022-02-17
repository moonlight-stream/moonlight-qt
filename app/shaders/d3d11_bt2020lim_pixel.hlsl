#include "d3d11_video_pixel_start.hlsli"

static const min16float3x3 cscMatrix =
{
    1.1644, 1.1644, 1.1644,
    0.0, -0.1874, 2.1418,
    1.6781, -0.6505, 0.0,
};

static const min16float3 offsets =
{
    16.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0
};

#include "d3d11_video_pixel_end.hlsli"