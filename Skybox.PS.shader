#META
stage:   PS
path:    Skybox/Skybox.PS.hlsl
profile: ps_6_0
#META_END

#HLSL
TextureCube<float4> gCube   : register(t0);
SamplerState        gSampler: register(s0);

struct Skybox
{
    float4x4 world;
    float4x4 viewProj;
    float    intensity;
    float3   pad;
};
ConstantBuffer<Skybox> gSkybox : register(b0);

struct VSOutput
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

float4 main(VSOutput i) : SV_TARGET
{
    float3 c = gCube.Sample(gSampler, normalize(i.dir)).rgb * gSkybox.intensity;
    return float4(c, 1.0f);
}
#HLSL_END