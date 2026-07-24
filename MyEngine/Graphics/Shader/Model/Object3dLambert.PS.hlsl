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
struct CameraData
{
    float32_t3 worldPosition;
    float padding;
};
ConstantBuffer<CameraData> gCamera : register(b2);

// ポイントライト
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float padA;
    float padB;
};
static const int kMaxPointLights = 16; // ライトの最大数
struct PointLightLists
{
    PointLight lights[kMaxPointLights];
    uint count; // 実際に有効な数
    float padA;
    float padB;
    float padC;
};
ConstantBuffer<PointLightLists> gPointLights : register(b3);

// テクスチャ
Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

// 円周率
static const float PI = 3.141596535f;


PixelShaderOutput main(VertexShaderOutput input)
{
    float3 N = normalize(input.normal);
    
    // 最終的な拡散光
    float3 diffuseLighting = { 0.0f, 0.0f, 0.0f };
    
    // ===== DirectionlLight =====
    {
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(N,lightDir));
        
        diffuseLighting += gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
    }
    
    // ===== PointLights =====
    {
        for (int i = 0; i < gPointLights.count; ++i)
        {
            PointLight light = gPointLights.lights[i];
            float3 lightDir = normalize(light.position - input.worldPosition);
            float NdotL = saturate(dot(N, lightDir));
            // 減衰
            float radius = max(light.radius, 0.0001f);
            float decay = max(light.decay, 0.0f);
            float distance = length(light.position - input.worldPosition);
            float factor = pow(saturate(-distance / radius + 1.0f), decay);
            
            diffuseLighting += light.color.rgb * light.intensity * NdotL * factor;
        }
    }
    
    // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
     // ライティングはRGBのみに適用する（alphaにcosを掛けると影の部分が透明になる）
    float32_t4 baseColor = gMaterial.color * texColor;
    float32_t4 finalColor;
    finalColor.rgb = baseColor.rgb * diffuseLighting;
    finalColor.a = baseColor.a;
   
    PixelShaderOutput output;
    output.color = finalColor;
    if (output.color.a == 0.0)
    {
        discard;
    }
    
    return output;
}
