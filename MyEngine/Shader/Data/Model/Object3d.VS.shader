#META
stage:   VS
path:    Model/Object3d.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Object3d.hlsli"
#include "Buffers/Camera.hlsli"
#include "Buffers/ObjectTransform.hlsli"


struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    float4 worldPos = mul(input.position, gObjectTransform.world);
    output.position = mul(worldPos, gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gObjectTransform.world));
    output.worldPosition = worldPos.xyz;
    return output;
}
#HLSL_END