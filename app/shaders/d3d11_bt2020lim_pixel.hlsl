Texture2D<min16float> luminancePlane : register(t0);
Texture2D<min16float2> chrominancePlane : register(t1);
SamplerState theSampler : register(s0);

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

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

min16float4 main(ShaderInput input) : SV_TARGET
{
    min16float3 yuv = min16float3(luminancePlane.Sample(theSampler, input.tex),
                                  chrominancePlane.Sample(theSampler, input.tex));

    // Subtract the YUV offset for limited vs full range
    yuv -= offsets;

    // Multiply by the conversion matrix for this colorspace
    yuv = mul(yuv, cscMatrix);

    return min16float4(yuv, 1.0);
}