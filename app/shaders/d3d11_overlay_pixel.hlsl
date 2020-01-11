Texture2D theTexture : register(t0);
SamplerState theSampler : register(s0);

struct ShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 main(ShaderInput input) : SV_TARGET
{
    return theTexture.Sample(theSampler, input.tex);
}