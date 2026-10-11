#META
stage:   PS
path:    Skybox/Skybox.PS.hlsl
profile: ps_6_0
#META_END

#HLSL
#include "Resources/Texture.hlsli"
#include "Resources/TextureCube.hlsli"

struct Skybox
{
    float4x4 world;
    float4x4 viewProj;
    float    intensity;
    uint     cubeIndex;
    float2   pad;
};
ConstantBuffer<Skybox> gSkybox : register(b20);

struct VSOutput
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float3 c = gTexturesCube[gSkybox.cubeIndex].Sample(gSampler, normalize(i.dir)).rgb * gSkybox.intensity;
    return float4(c, 1.0f);
}
#HLSL_END