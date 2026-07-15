#include "Object3d.hlsli"

struct ModelMaterial {
    float32_t4   color;
    float4x4 uvTransform;
    float32_t3   ambient;    float padA;
    float32_t3   diffuse;    float padD;
    float32_t3   specular;   float shininess;
    float32_t3   emissive;   uint textureIndex;
};
ConstantBuffer<ModelMaterial> gMaterial : register(b0);

struct DirectionalLight {
    float32_t4 color;
    float32_t3 direction;
    float      intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera {
    float32_t3 worldPosition;
    float      padding;
};
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    // HalfLambert
    float32_t3 N = normalize(input.normal);
    float32_t3 L = -normalize(gDirectionalLight.direction);
    float cos = pow(dot(N, L) * 0.5f + 0.5f, 2.0f);
    // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    // Output
    // 使わないものは無効化する
    output.color.rgb += sign(gMaterial.ambient) * sign(gMaterial.diffuse) * sign(gMaterial.emissive)
    * sign(gMaterial.shininess) * sign(gMaterial.specular) * 0.0f;
    output.color.rgb = gMaterial.color.rgb * texColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    output.color.a = gMaterial.color.a * texColor.a;
    if (output.color.a == 0.0)
    {
        discard;
    }
    
    return output;
}
