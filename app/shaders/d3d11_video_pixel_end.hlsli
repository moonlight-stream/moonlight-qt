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