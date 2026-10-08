// Pixel Shader
// Convert YUV -> Linear RGB
// Supports NV12, P010, AYUV, Y410 input
// Compile with Shader 6.2 and "-enable-16bit-types"

// Input format enum
#define FMT_NV12   0
#define FMT_P010   1
#define FMT_AYUV   2
#define FMT_Y410   3

// Gamma / color space
#define GC_LINEAR  0
#define GC_G22     1
#define GC_G24     2
#define GC_PQ      3

// Output enum
#define OUT_RGBA8  0
#define OUT_RGB10  1

// Color range
#define CR_LIMITED  0
#define CR_FULL    1

// Root constants: small cbuffer (SetGraphicsRoot32BitConstants alternative)
cbuffer RootConsts : register(b0)
{
    // SDR and HDR invert bits
    // Block 0 (0->16)
    float g_INV_8BIT  : packoffset(c0.x);
    float g_INV_10BIT : packoffset(c0.y);

    // PQ converter constants
    // Block 1 (16->32)
    float g_M1Inv : packoffset(c1.x);
    float g_M2Inv : packoffset(c1.y);
    float g_C1    : packoffset(c1.z);
    float g_C2    : packoffset(c1.w);
    // Block 2 (32->48)
    float g_C3    : packoffset(c2.x);

    // CSC matrix rows
    // Block 3 (48->64)
    float g_CSC_Row0_x : packoffset(c3.x);
    float g_CSC_Row0_y : packoffset(c3.y);
    float g_CSC_Row0_z : packoffset(c3.z);

    // Block 4 (64->80)
    float g_CSC_Row1_x : packoffset(c4.x);
    float g_CSC_Row1_y : packoffset(c4.y);
    float g_CSC_Row1_z : packoffset(c4.z);

    // Block 5 (80->96)
    float g_CSC_Row2_x : packoffset(c5.x);
    float g_CSC_Row2_y : packoffset(c5.y);
    float g_CSC_Row2_z : packoffset(c5.z);

    // Color range and YUV Offset
    // Block 6 (96->112)
    float g_ScaleY  : packoffset(c6.x);
    float g_OffsetY : packoffset(c6.y);
    float g_OffsetU : packoffset(c6.z);
    float g_OffsetV : packoffset(c6.w);

    // Texture format In/Out
    // Block 7 (112->128)
    uint g_InputFormat     : packoffset(c7.x);
    uint g_OutputFormat    : packoffset(c7.y);
    uint g_GammaCorrection : packoffset(c7.z);
    uint g_Range           : packoffset(c7.w);
};

// Texture2D Plane Y (luminance) or AYUV/Y410
Texture2D<float> g_TexY     : register(t0);

// Texture2D Plane UV (chrominance)
Texture2D<float2> g_TexUV   : register(t1);

// Sampler Point for perfomance
SamplerState g_SamplerPoint : register(s0);

// ============================================================================
// VERTEX SHADER OUTPUT STRUCTURE
// ============================================================================

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// ============================================================================
// VERTEX SHADER
// ============================================================================

VS_OUTPUT mainVS(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;

    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

    return output;
}

// ============================================================================
// PIXEL SHADER (Convert YUV -> RGB + Gamma -> Linear)
// ============================================================================

#if ADVANCED_SHADER

    float4 mainPS(VS_OUTPUT input) : SV_TARGET
    {
        // Convert to half
        float16_t g16_INV_8BIT = (float16_t)g_INV_8BIT;
        float16_t g16_INV_10BIT = (float16_t)g_INV_10BIT;
    
        float16_t g16_M1Inv = (float16_t)g_M1Inv;
        float16_t g16_M2Inv = (float16_t)g_M2Inv;
        float16_t g16_C1    = (float16_t)g_C1;
        float16_t g16_C2    = (float16_t)g_C2;
        float16_t g16_C3    = (float16_t)g_C3;
    
        float16_t g16_CSC_Row0_x = (float16_t)g_CSC_Row0_x;
        float16_t g16_CSC_Row0_y = (float16_t)g_CSC_Row0_y;
        float16_t g16_CSC_Row0_z = (float16_t)g_CSC_Row0_z;
    
        float16_t g16_CSC_Row1_x = (float16_t)g_CSC_Row1_x;
        float16_t g16_CSC_Row1_y = (float16_t)g_CSC_Row1_y;
        float16_t g16_CSC_Row1_z = (float16_t)g_CSC_Row1_z;
    
        float16_t g16_CSC_Row2_x = (float16_t)g_CSC_Row2_x;
        float16_t g16_CSC_Row2_y = (float16_t)g_CSC_Row2_y;
        float16_t g16_CSC_Row2_z = (float16_t)g_CSC_Row2_z;
    
        float16_t g16_ScaleY  = (float16_t)g_ScaleY;
        float16_t g16_OffsetY = (float16_t)g_OffsetY;
        float16_t g16_OffsetU = (float16_t)g_OffsetU;
        float16_t g16_OffsetV = (float16_t)g_OffsetV;
    
        uint16_t g16_InputFormat     = (uint16_t)g_InputFormat;
        uint16_t g16_OutputFormat    = (uint16_t)g_OutputFormat;
        uint16_t g16_GammaCorrection = (uint16_t)g_GammaCorrection;
        uint16_t g16_Range           = (uint16_t)g_Range;
    
        // sample into float then convert to half (texture fetch typically returns float)
        float yf = 0.0f;
        float2 uvf = float2(0.0f, 0.0f);
    
        if (g16_InputFormat == FMT_NV12)
        {
            yf  = g_TexY.Sample(g_SamplerPoint, input.texCoord).r;
            uvf = g_TexUV.Sample(g_SamplerPoint, input.texCoord).rg;
        }
        else if (g_InputFormat == FMT_P010)
        {
            yf  = g_TexY.Sample(g_SamplerPoint, input.texCoord).r;
            uvf = g_TexUV.Sample(g_SamplerPoint, input.texCoord).rg;
        }
        else if (g16_InputFormat == FMT_AYUV)
        {
            float4 packed = g_TexY.Sample(g_SamplerPoint, input.texCoord);
            yf  = packed.b;
            uvf = packed.gr;
        }
        else if (g16_InputFormat == FMT_Y410)
        {
            float4 packed = g_TexY.Sample(g_SamplerPoint, input.texCoord);
            yf  = packed.g;
            uvf = packed.rb;
        }
        else
        {
            return float4(0.0f, 1.0f, 0.0f, 1.0f);
        }
    
        // Convert into half domain for processing
        float16_t y = float16_t(yf);
        float16_t2 uv = float16_t2(uvf);
        // Range conversion: limited [16..235] -> [0..1] if required
        if (g16_Range == CR_LIMITED)
        {
            y = y - float16_t(g16_OffsetY);
        }
    
        // Subtract offsets (offsets stored in cbuffer as min16float)
        float16_t u = uv.x - float16_t(g16_OffsetU);
        float16_t v = uv.y - float16_t(g16_OffsetV);
        float16_t3 yuv = float16_t3(y, u, v);
    
        // Multiply by CSC (Y'CbCr -> R'G'B')
        float16_t3 rgbPrime;
        rgbPrime.r = dot(yuv, float16_t3(g16_CSC_Row0_x, g16_CSC_Row0_y, g16_CSC_Row0_z));
        rgbPrime.g = dot(yuv, float16_t3(g16_CSC_Row1_x, g16_CSC_Row1_y, g16_CSC_Row1_z));
        rgbPrime.b = dot(yuv, float16_t3(g16_CSC_Row2_x, g16_CSC_Row2_y, g16_CSC_Row2_z));
    
        return float4((float16_t3)saturate(rgbPrime), 1.0f);
    }

#else

    float4 mainPS(VS_OUTPUT input) : SV_TARGET
    {
        float yf = 0.0f;
        float2 uvf = float2(0.0f, 0.0f);
    
        if (g_InputFormat == FMT_NV12)
        {
            yf  = g_TexY.Sample(g_SamplerPoint, input.texCoord).r;
            uvf = g_TexUV.Sample(g_SamplerPoint, input.texCoord).rg;
        }
        else if (g_InputFormat == FMT_P010)
        {
            yf  = g_TexY.Sample(g_SamplerPoint, input.texCoord).r;
            uvf = g_TexUV.Sample(g_SamplerPoint, input.texCoord).rg;
        }
        else if (g_InputFormat == FMT_AYUV)
        {
            float4 packed = g_TexY.Sample(g_SamplerPoint, input.texCoord);
            yf  = packed.b;
            uvf = packed.gr;
        }
        else if (g_InputFormat == FMT_Y410)
        {
            float4 packed = g_TexY.Sample(g_SamplerPoint, input.texCoord);
            yf  = packed.g;
            uvf = packed.rb;
        }
        else
        {
            return float4(0.0f, 1.0f, 0.0f, 1.0f);
        }
    
        // Convert into half domain for processing
        float y = float(yf);
        float2 uv = float2(uvf);
        // Range conversion: limited [16..235] -> [0..1] if required
        if (g_Range == CR_LIMITED)
        {
            y = y - float(g_OffsetY);
        }
    
        // Subtract offsets (offsets stored in cbuffer as min16float)
        float u = uv.x - float(g_OffsetU);
        float v = uv.y - float(g_OffsetV);
        float3 yuv = float3(y, u, v);
    
        // Multiply by CSC (Y'CbCr -> R'G'B')
        float3 rgbPrime;
        rgbPrime.r = dot(yuv, float3(g_CSC_Row0_x, g_CSC_Row0_y, g_CSC_Row0_z));
        rgbPrime.g = dot(yuv, float3(g_CSC_Row1_x, g_CSC_Row1_y, g_CSC_Row1_z));
        rgbPrime.b = dot(yuv, float3(g_CSC_Row2_x, g_CSC_Row2_y, g_CSC_Row2_z));
    
        return float4((float3)saturate(rgbPrime), 1.0f);
    }

#endif


