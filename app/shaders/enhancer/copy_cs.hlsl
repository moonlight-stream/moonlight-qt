// Copy RGBA to RGBA

Texture2D<float4> g_Input : register(t0);
RWTexture2D<float4> g_Output : register(u0);

[numthreads(16, 16, 1)]
void mainCS(uint3 DTid : SV_DispatchThreadID)
{
    g_Output[DTid.xy] = g_Input.Load(int3(DTid.xy, 0));
}
