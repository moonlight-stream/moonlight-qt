Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

struct PSInput {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput pin) : SV_TARGET {
    float4 texColor = tex0.Sample(samp0, pin.uv);
    return lerp(pin.color, texColor, texColor.a);
}
