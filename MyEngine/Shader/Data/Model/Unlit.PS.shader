#META
stage:   PS
path:    Model/Unlit.PS.hlsl
profile: ps_6_0
#META_END

#PROGRAM
category: Model
shading:  Unlit
vs:       Object3dVS
#PROGRAM_END

#HLSL
#include "Object3d.hlsli"
#include "Resources/Texture.hlsli"

struct ModelMaterial {
    float32_t4   color;
    float4x4 uvTransform;
    float32_t3   ambient;    float padA;
    float32_t3   diffuse;    float padD;
    float32_t3   specular;   float shininess;
    float32_t3   emissive;   uint textureIndex;
};
ConstantBuffer<ModelMaterial> gMaterial : register(b20);


PixelShaderOutput main(VertexShaderOutput input) {
    // Texture
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 texColor      = gTextures[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);
    if (texColor.a == 0.0)
    {
        discard;
    }
    
    // Color
    float32_t3 finalColor = gMaterial.color.rgb * texColor.rgb;
    float       finalAlpha = gMaterial.color.a * texColor.a;

    PixelShaderOutput output;
    output.color = float32_t4(finalColor, finalAlpha);
    if (output.color.a == 0.0)
    {
        discard;
    }
    return output;
}

#HLSL_END