#include "d3d11_video_pixel_start.hlsli"

static const min16float3x3 cscMatrix =
{
    1.1678, 1.1678, 1.1678,
    0.0, -0.1879, 2.1481,
    1.6836, -0.6524, 0.0,
};

static const min16float3 offsets =
{
    64.0 / 1023.0, 512.0 / 1023.0, 512.0 / 1023.0
};

#include "d3d11_video_pixel_end.hlsli"