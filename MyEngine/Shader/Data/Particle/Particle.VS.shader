#META
stage:   VS
path:    Particle/Particle.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Particle.hlsli"
#include "MyEngine/Shader/Data/Buffers/Camera.hlsli"


struct Particle
{
    float4x4 wvp;
    float4x4 world;
    float32_t4 color;
};
StructuredBuffer<Particle> gParticles : register(t0);


struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};


VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    float4 worldPos = mul(input.position, gParticles[instanceId].world);
    output.position = mul(worldPos, gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.color = gParticles[instanceId].color;
    return output;
}
#HLSL_END