#include "Object3d.hlsli"

struct ModelMaterial
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
ConstantBuffer<ModelMaterial> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct CameraData
{
    float32_t3 worldPosition;
    float padding;
};
ConstantBuffer<CameraData> gCamera : register(b2);

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    // Lambert
    float32_t3 N = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);
    float cos = saturate(dot(N, L));
    // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a)
    {
        discard;
    }
    
    float32_t3 diffuseCol = gMaterial.diffuse * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    float32_t3 ambientCol = gMaterial.ambient;
    float32_t3 emissiveCol = gMaterial.emissive;

    float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);
    float32_t3 H = normalize(L + V);
    float32_t3 specularCol = gMaterial.specular
        * gDirectionalLight.color.rgb
        * pow(saturate(dot(N, H)), gMaterial.shininess)
        * gDirectionalLight.intensity;

    float32_t3 finalColor = (diffuseCol + ambientCol + emissiveCol + specularCol) * gMaterial.color.rgb * texColor.rgb;
    float finalAlpha = gMaterial.color.a * texColor.a;

    PixelShaderOutput output;
    output.color = float32_t4(finalColor, finalAlpha);
    if (output.color.a)
    {
        discard;
    }
    
    return output;
}
