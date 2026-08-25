#META
stage: PS
path : Post/BloomDown.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
#include "Post/Bloom.hlsli"

float32_t4 main(VSOutput input) : SV_TARGET
{
    return float32_t4(Tent(gBloom.srcIndex, input.texcoord, gBloom.texelSize), 1.0f);
}
#HLSL_END