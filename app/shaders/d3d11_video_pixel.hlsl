Texture2D luminancePlane : register(t0);
Texture2D chrominancePlane : register(t1);
SamplerState theSampler : register(s0);

cbuffer CSC_CONST_BUF : register(b0)
{
    float3x3 cscMatrix;
    float3 offsets;
};

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

min16float4 main(ShaderInput input) : SV_TARGET
{
    float y = luminancePlane.Sample(theSampler, input.tex);
    float2 uv = chrominancePlane.Sample(theSampler, input.tex);
    float3 yuv = float3(y, uv);

    // Subtract the YUV offset for limited vs full range
    yuv -= offsets;

    // Multiply by the conversion matrix for this colorspace
    yuv = mul(yuv, cscMatrix);

    return min16float4(saturate(yuv), 1.0f);
}