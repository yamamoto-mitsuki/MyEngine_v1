#include "Object3d.hlsli"

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

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float32_t3 worldPosition;
    float padding;
};
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

static const float PI = 3.14159265359f;

//=============================================================================
// D項: 法線分布関数（GGX/Trowbridge-Reitz）       ラフネル？
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