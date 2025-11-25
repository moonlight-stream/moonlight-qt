Texture2D<min16float3> videoTex : register(t0);
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
    min16float2 chromaOffset; // Unused for 4:4:4
    min16float2 chromaTexMax; // Unused for 4:4:4
};