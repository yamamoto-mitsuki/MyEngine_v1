#include "Object3d.hlsli"

struct ModelMaterial {
    float32_t4   color;
    float32_t4x4 uvTransform;
    float32_t3   ambient;    float padA;
    float32_t3   diffuse;    float padD;
    float32_t3   specular;   float shininess;
    float32_t3   emissive;   float padE;
};
ConstantBuffer<ModelMaterial> gMaterial : register(b0);

#ifdef USE_TEXTURE
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
#endif

PixelShaderOutput main(VertexShaderOutput input) {
#ifdef USE_TEXTURE
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTexture.Sample(gSampler, transformedUV.xy);
#else
    float32_t4 texColor = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

    float32_t3 finalColor = gMaterial.color.rgb * texColor.rgb;
    float       finalAlpha = gMaterial.color.a * texColor.a;

    PixelShaderOutput output;
    output.color = float32_t4(finalColor, finalAlpha);
    return output;
}
