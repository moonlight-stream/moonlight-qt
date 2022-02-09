struct ShaderInput
{
    min16float2 pos : POSITION;
    min16float2 tex : TEXCOORD0;
};

struct ShaderOutput
{
    min16float4 pos : SV_POSITION;
    min16float2 tex : TEXCOORD0;
};

ShaderOutput main(ShaderInput input) 
{
    ShaderOutput output;
    output.pos = min16float4(input.pos, 0.0, 1.0);
    output.tex = input.tex;
    return output;
}