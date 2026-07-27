#META
stage:   PS
path:    Model/Lambert.PS.hlsl
profile: ps_6_0
#META_END

#PROGRAM
category: Model
shading:  Lambert
vs:       Object3dVS
#PROGRAM_END

#HLSL
#include "Object3d.hlsli"
#include "Buffers/Camera.hlsli"
#include "Buffers/Light.hlsli"
#include "Resources/Texture.hlsli"

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
ConstantBuffer<Material> gMaterial : register(b20);

static const float PI = 3.141596535f;


PixelShaderOutput main(VertexShaderOutput input)
{
    float3 N = normalize(input.normal);
    float3 diffuseLighting = { 0.0f, 0.0f, 0.0f };

    {
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(N,lightDir));
        diffuseLighting += gDirectionalLight.color.rgb * gDirectionalLight.intensity * NdotL;
    }
    {
        for (int i = 0; i < gPointLights.count; ++i)
        {
            PointLight light = gPointLights.lights[i];
            float3 lightDir = normalize(light.position - input.worldPosition);
            float NdotL = saturate(dot(N, lightDir));
            float radius = max(light.radius, 0.0001f);
            float decay = max(light.decay, 0.0f);
            float distance = length(light.position - input.worldPosition);
            float factor = pow(saturate(-distance / radius + 1.0f), decay);
            diffuseLighting += light.color.rgb * light.intensity * NdotL * factor;
        }
    }

    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0) { discard; }
    float32_t4 baseColor = gMaterial.color * texColor;
    float32_t4 finalColor;
    finalColor.rgb = baseColor.rgb * diffuseLighting;
    finalColor.a = baseColor.a;

    PixelShaderOutput output;
    output.color = finalColor;
    if (output.color.a == 0.0) { discard; }
    return output;
}
#HLSL_END