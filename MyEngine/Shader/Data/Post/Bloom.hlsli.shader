#META
stage: Include
path : Post/Bloom.hlsli
#META_END

#HLSL
#include "Resources/Texture.hlsli"

struct BloomParam
{
    float32_t2 texelSize; // 1 / ソースの解像度
    float threshold;      // これより明るい所だけ光らせる
    float knee;           // しきい値付近の柔らかさ
    float intensity;      // 合成の強さ
    uint32_t srcIndex;    // ソースのSRVスロット
    uint32_t addIndex;    // 足し込む側のSRVスロット
    float padding;
};
ConstantBuffer<BloomParam> gBloom : register(b0);

struct VSOutput
{
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
};

// 3x3のテント型フィルタ。1回のパスで滑らかにぼかす
float32_t3 Tent(uint32_t index, float32_t2 uv, float32_t2 texel)
{
    float32_t3 sum = float32_t3(0, 0, 0);
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(-texel.x, -texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(0.0f, -texel.y)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(texel.x, -texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(-texel.x, 0.0f)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSampler, uv).rgb * 4.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(texel.x, 0.0f)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(-texel.x, texel.y)).rgb * 1.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(0.0f, texel.y)).rgb * 2.0f;
    sum += gTextures[index].Sample(gSampler, uv + float32_t2(texel.x, texel.y)).rgb * 1.0f;
    return sum * (1.0f / 16.0f);
}
#HLSL_END