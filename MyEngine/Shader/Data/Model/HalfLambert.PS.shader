#META
stage:   PS
path:    Model/HalfLambert.PS.hlsl
profile: ps_6_0
#META_END

#PROGRAM
category: Model
shading:  HalfLambert
vs:       Object3dVS
#PROGRAM_END

#HLSL
#include "Object3d.hlsli"
#include "MyEngine/Shader/Data/Buffers/Camera.hlsli"
#include "MyEngine/Shader/Data/Buffers/Light.hlsli"
#include "MyEngine/Shader/Data/Resources/Texture.hlsli"

// マテリアル
struct ModelMaterial {
    float32_t4   color;
    float4x4 uvTransform;
    float32_t3   ambient;    float padA;
    float32_t3   diffuse;    float padD;
    float32_t3   specular;   float shininess;
    float32_t3   emissive;   uint textureIndex;
};
ConstantBuffer<ModelMaterial> gMaterial : register(b2);

// 円周率
static const float PI = 3.14159265f;


PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float3 N = normalize(input.normal);
    // 最終的な拡散光
    float3 diffuseLighting = { 0.0f, 0.0f, 0.0f };
    
    // ===== DirectionlLight =====
    {
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float cos = pow(dot(N, lightDir) * 0.5f + 0.5f, 2.0f);
        
        diffuseLighting += gDirectionalLight.color.rgb * gDirectionalLight.intensity * cos;
    }
    
    // ===== PointLights =====
    {
        for (int i = 0; i < gPointLights.count;  ++i)
        {
            PointLight light = gPointLights.lights[i];
            float3 lightDir = normalize(input.worldPosition - light.position);
            lightDir = normalize(light.position - input.worldPosition);
            float cos = pow(dot(N, lightDir) * 0.5f + 0.5f, 2.0f);
            // 減衰
            float radius = max(light.radius, 0.0001f);
            float decay = max(light.decay, 0.0f);
            float distance = length(light.position - input.worldPosition);
            float factor = pow(saturate(-distance / radius + 1.0f), decay);
            
            diffuseLighting += light.color.rgb * light.intensity * cos * factor;
        }
    }
     
    // ===== Texture =====
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    // Output
    output.color.rgb = gMaterial.color.rgb * texColor.rgb * diffuseLighting;
    output.color.a = gMaterial.color.a * texColor.a;
    if (output.color.a == 0.0)
    {
        discard;
    }
    
    return output;
}

#HLSL_END
