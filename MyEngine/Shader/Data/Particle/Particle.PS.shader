#META
stage:        PS
drawCategory: Particle
shading:      Unlit
vs:           ParticleVS
path:         Particle/Particle.PS.hlsl
profile:      ps_6_0
#META_END

#HLSL
#include "Particle.hlsli"

struct Material
{
    float32_t4 color;
    float4x4 uvTransform;
    uint32_t textureIndex;
    float padA;
    float padB;
    float padC;
};
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);


PixelShaderOutput main(VertexShaderOutput input)
{
  // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    
    // Color
    float32_t3 finalColor = input.color.rgb * gMaterial.color.rgb * texColor.rgb;
    float finalAlpha = input.color.a * gMaterial.color.a * texColor.a;

    PixelShaderOutput output;
    output.color = float32_t4(finalColor, finalAlpha);
    if (output.color.a == 0.0)
    {
        discard;
    }
    return output;
}
#HLSL_END