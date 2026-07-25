#META
stage:        PS
drawCategory: Model
shading:      Phong
vs:           Object3dVS
path:         Model/Phong.PS.hlsl
profile:      ps_6_0
#META_END

#HLSL
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



PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 N = normalize(input.normal);
    // 最終的な拡散光, 鏡面反射
    float3 diffuseLighting = { 0.0f, 0.0f, 0.0f };
    float3 specularLighting = { 0.0f, 0.0f, 0.0f };
    
    
    // ===== DirectionlLight =====
    {
        // --- Lambert ---
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(N, L));
        // 拡散反射
        diffuseLighting += gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
        
        // --- Phong ---
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float3 reflectLight = reflect(-L, N);
        float RdotE = dot(reflectLight, toEye);
        float specularPow = pow(saturate(RdotE), gMaterial.shininess); // 反射強度
        // 鏡面反射
        specularLighting += gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow;
    }
    
    // ===== PointLight =====
    {
        for (int i = 0; i < gPointLights.count; ++i)
        {
            // 1つ分のライト
            PointLight light = gPointLights.lights[i];
            
            // --- 減衰（radiusで0になる） ---
            float radius = max(light.radius, 0.0001f);
            float decay = max(light.decay, 0.0f);
            float distance = length(light.position - input.worldPosition);
            float factor = pow(saturate(-distance / radius + 1.0f), decay);
            
            // --- Lambert ---
            float3 L = normalize(light.position - input.worldPosition);
            float NdotL = saturate(dot(N, L));
            // 拡散反射
            diffuseLighting += light.color.rgb * NdotL * light.intensity * factor;
            
            // --- Phong ---
            float3 V = normalize(gCamera.worldPosition - input.worldPosition);
            float3 reflectionLight = reflect(L, N);
            float RdotE = dot(-reflectionLight, V);
            float specularPow = pow(saturate(RdotE), gMaterial.shininess); // 反射強度
            // 鏡面反射
            specularLighting += light.color.rgb * light.intensity * specularPow * factor;
        }
    }
    
    // ===== Texture =====
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    
    // ===== 出力色 =====
    //diffuseLighting *= gMaterial.diffuse;
    //specularLighting *= gMaterial.specular;
    specularLighting *= float3(1.0f,1.0f,1.0f);
    
    output.color.rgb = (diffuseLighting + specularLighting) * gMaterial.color.rgb * texColor.rgb; // 拡散反射 + 鏡面反射
    output.color.a = gMaterial.color.a * texColor.a;
    if (output.color.a == 0.0)
    {
        discard;
    }
    return output;
}
#HLSL_END