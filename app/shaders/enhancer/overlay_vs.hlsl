struct VSInput {
    float2 pos   : POSITION0;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct PSInput {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput main(VSInput vin) {
    PSInput vout;
    vout.pos = float4(vin.pos, 0.0f, 1.0f);
    vout.uv  = vin.uv;
    vout.color = vin.color;
    return vout;
}
