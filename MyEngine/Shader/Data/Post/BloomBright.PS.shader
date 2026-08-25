#META
stage: PS
path : Post/BloomBright.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
#include "Post/Bloom.hlsli"

float32_t4 main(VSOutput input) : SV_TARGET
{
    float32_t3 color = gTextures[gBloom.srcIndex].Sample(gSampler, input.texcoord).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    // しきい値でバッサリ切るとフレーム間でチラつくので、knee の幅だけ滑らかに繋ぐ
    float soft = clamp(brightness - gBloom.threshold + gBloom.knee, 0.0f, 2.0f * gBloom.knee);
    soft = soft * soft / (4.0f * gBloom.knee + 0.0001f);
    float contribution = max(soft, brightness - gBloom.threshold) / max(brightness, 0.0001f);
    return float32_t4(color * contribution, 1.0f);
}
#HLSL_END