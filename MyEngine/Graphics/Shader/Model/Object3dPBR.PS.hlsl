#include "Object3d.hlsli"

// マテリアル
struct Material
{
    float32_t4 color;
    float4x4 uvTransform;
    float32_t3 ambient;
    float padA;
    float32_t3 diffuse;
    float padD;
    float32_t3 specular;
    float shininess;
    float32_t3 emissive;
    uint textureIndex;
    float metallic;
    float roughness;
    float padP0;
    float padP1;
};
ConstantBuffer<Material> gMaterial : register(b0);

// 平行光源
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

// カメラ
struct Camera
{
    float32_t3 worldPosition;
    float padding;
};
ConstantBuffer<Camera> gCamera : register(b2);

// ポイントライト
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
};
static const int kMaxPointLights = 16; // ライトの最大数
struct PointLightLists
{
    PointLight lights[kMaxPointLights];
    int count; // 実際に有効な数
};
ConstantBuffer<PointLightLists> gPointLights : register(b3);

// テクスチャ
Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

static const float PI = 3.14159265359f;

//=============================================================================
// D項: 法線分布関数（GGX/Trowbridge-Reitz）
// マイクロファセット（微細な凹凸面）のうち、どれだけがハーフベクトルHの方向を
// 向いているかの密度。これがハイライトの「形」を決める。
// roughnessが小さい→分布が尖る→鋭く明るいハイライト
// roughnessが大きい→分布が広がる→ぼんやり広いハイライト
//=============================================================================
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 0.0001f); // 0除算防止
}

//=============================================================================
// G項: 幾何減衰（Smith法 + Schlick-GGX近似）     粗さの計算
// マイクロファセット同士が光を遮り合う効果。粗い面ほど、また視線/ライトが
// 浅い角度になるほど、凹凸の影で反射が減る。
// これが無いと、粗い面や輪郭付近が物理的にありえない明るさになる。
//=============================================================================
float GeometrySchlickGGX(float NdotX, float roughness) {
    // 直接光用のリマップ
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}
float GeomtrySmith(float NdotV, float NdotL, float roughness) {
    // 視線方向の遮蔽 * ライトの方向の影 の両方を考慮
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

//=============================================================================
// F項: フレネル反射（Schlick近似）
// 見る角度が浅くなるほど反射率が上がる現象。F0は「正面から見たときの反射率」。
// 非金属: F0はほぼ無彩色の4%。金属: F0がアルベド色そのもの（金なら黄色い反射）
//=============================================================================
float32_t3 FresnalSchlick(float VdotH, float32_t3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f -VdotH), 5.0f);
}


PixelShaderOutput main(VertexShaderOutput input)
{
    // 最終的な光
    
    
    // ===== 使用ベクトル =====
    float32_t3 N = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);
    float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition); // 視点へ向かう
    float32_t3 H = normalize(L + V); // ハーフベクトル
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // ===== アルベド（素の色） =====
    float32_t4 transformdUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformdUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    float32_t3 albedo = gMaterial.color.rgb * texColor.rgb;
    
    // ===== metallic計算 =====
    // 非金属(metallic=0): F0=4%グレー、albedoは拡散反射として使う
    // 金属(metallic=1):   F0=albedo（反射に色がつく）、拡散は0
    float32_t3 F0 = lerp(float32_t3(0.04f, 0.04f, 0.04f), albedo, gMaterial.metallic);
    
    // ===== Cook-Torrance スペキュラBRDF: D*G*F / (4 * NdotL * NdotV) =====
    float D = DistributionGGX(NdotH, gMaterial.roughness);
    float G = GeomtrySmith(NdotV, NdotL, gMaterial.roughness);
    float32_t3 F = FresnalSchlick(VdotH, F0);
    float32_t3 specular = (D * G * F) / max(4.0f * NdotL * NdotV, 0.00001f);

    // ===== 拡散（エネルギー保存則） =====
    // Fで反射に使われた光の残り（1 - F）だけが拡散に回る。金属は拡散ゼロ
    // / PI はLambert拡散の正規化（半球だけ積分すると1になる）
    float32_t3 kD = (1.0f - F) * (1.0f - gMaterial.metallic);
    float32_t3 diffuse = kD * albedo / PI;
    
    // ===== 出射輝度 =====
    float32_t3 radiance = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
    float32_t3 Lo = (diffuse + specular) * radiance * NdotL;
    // 環境光は今は定数で代用
    float32_t3 ambient = gMaterial.ambient * albedo;
    
    
    PixelShaderOutput output;
    output.color = float32_t4(Lo + ambient + gMaterial.emissive, gMaterial.color.a * texColor.a);
    if (output.color.a == 0.0)
    {
        discard;
    }
    return output;
}