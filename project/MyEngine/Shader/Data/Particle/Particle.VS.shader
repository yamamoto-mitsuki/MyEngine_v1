#META
stage:   VS
path:    Particle/Particle.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Particle.hlsli"
#include "Buffers/Camera.hlsli"


struct Particle
{
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
    float4x4 world = gParticles[instanceId].world;
    // world から中心位置とスケールを取り出す
    float3 center = float3(world._41, world._42, world._43);
    float sx = length(float3(world._11, world._12, world._13));
    float sy = length(float3(world._21, world._22, world._23));
    // カメラ基底でクアッドを組む
    float3 worldPos = center + gCamera.right * (input.position.x * sx)
                             + gCamera.up * (input.position.y * sy);
    output.position = mul(float4(worldPos, 1.0f), gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.color = gParticles[instanceId].color;
    return output;
}
#HLSL_END