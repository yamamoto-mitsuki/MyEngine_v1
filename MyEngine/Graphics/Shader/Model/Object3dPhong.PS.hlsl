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

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    // HalfLambert
    float32_t3 N = normalize(input.normal);
    float32_t3 L = normalize(-gDirectionalLight.direction);
    float NdotL = pow(dot(N, L) * 0.5f + 0.5f, 2.0f);
    // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    // --- Phong ---
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    float32_t3 reflectLight = reflect(-L, normalize(input.normal));
    float RdotE = dot(reflectLight, toEye);
    float specularPow = pow(saturate(RdotE), gMaterial.shininess); // 反射強度
    // 拡散反射
    float32_t3 diffuse = gMaterial.color.rgb * texColor.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
    // 鏡面反射
    float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * gMaterial.specular.rgb;
    // 拡散反射 + 鏡面反射
    output.color.rgb = diffuse + specular;
    output.color.a = gMaterial.color.a * texColor.a;
    if (output.color.a == 0.0)
    {
        discard;
    }
    return output;
}