#include "Sprite2d.hlsli"

struct Material {
    float32_t4 color;
    float32_t4x4 uvTransform;
    uint textureIndex;        float32_t3 padM;
};
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
   PixelShaderOutput output;
   float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
   float32_t4 textureColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
   output.color = gMaterial.color * textureColor;
   return output;
}