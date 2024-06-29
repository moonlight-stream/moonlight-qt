Texture2D<min16float> luminancePlane : register(t0);
Texture2D<min16float2> chrominancePlane : register(t1);
SamplerState theSampler : register(s0);

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

cbuffer ChromaLimitBuf : register(b0)
{
    min16float3 chromaTexMax;
};
