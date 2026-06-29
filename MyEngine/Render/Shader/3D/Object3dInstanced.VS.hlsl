#include "Object3d.hlsli"

struct InstanceData {
    float4x4 WVP;
    float4x4 World;
};
StructuredBuffer<InstanceData> gInstances : register(t0, space1);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID) {
    InstanceData inst = gInstances[instanceID];

    VertexShaderOutput output;
    output.position = mul(input.position, inst.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) inst.World));
    output.worldPosition = mul(input.position, inst.World).xyz;
    return output;
}