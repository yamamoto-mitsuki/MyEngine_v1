#META
stage: PS
path : Post/Lens.PS.hlsl
profile : ps_6_0
#META_END

#HLSL
#include "Resources/Texture.hlsli"

struct LensParam
{
    float distortion; // 画面端の歪み。正で外へ膨らむ
    float aberration; // 色収差。端ほどRGBがずれる
    float radialBlur; // 中心へ向かって流れるブラー
    float vignette;   // 端の暗さ
    uint32_t srcIndex;
    float3 padding;
};
ConstantBuffer<LensParam> gLens : register(b0);

struct VSOutput
{
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
};

float32_t4 main(VSOutput input) : SV_TARGET
{
    float32_t2 uv = input.texcoord;
    float32_t2 offset = uv - 0.5f; // 画面中心からのずれ
    float dist2 = dot(offset, offset); // 中心=0、四隅=0.5。二乗なので端だけ強く効く

    // ===== 歪み：中心はそのまま、端ほど押し出す =====
    float32_t2 warped = saturate(uv + offset * dist2 * gLens.distortion);

    // ===== 色収差：RGBを歪みと同じ向きにずらす =====
    float shift = gLens.aberration * dist2;
    float32_t3 color;
    color.r = gTextures[gLens.srcIndex].Sample(gSampler, saturate(warped + offset * shift)).r;
    color.g = gTextures[gLens.srcIndex].Sample(gSampler, warped).g;
    color.b = gTextures[gLens.srcIndex].Sample(gSampler, saturate(warped - offset * shift)).b;

    // ===== 放射ブラー：中心へ向かって数回サンプルして重ねる =====
    // 外へ向かって拾うと画面外を舐めるので、必ず中心側へ引く
    if (gLens.radialBlur > 0.001f)
    {
        float32_t3 sum = color;
        for (int i = 1; i < 6; ++i)
        {
            float t = float(i) / 5.0f;
            sum += gTextures[gLens.srcIndex].Sample(gSampler, saturate(warped - offset * (t * gLens.radialBlur))).rgb;
        }
        color = sum * (1.0f / 6.0f);
    }

    // ===== ビネット：端を落として中心へ視線を集める =====
    color *= 1.0f - saturate(dist2 * gLens.vignette);

    return float32_t4(color, 1.0f);
}
#HLSL_END