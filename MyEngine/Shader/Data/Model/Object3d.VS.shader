#META
stage:   VS
path:    Model/Object3d.VS.hlsl
profile: vs_6_0
#META_END

#HLSL
#include "Object3d.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position      = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord      = input.texcoord;
    output.normal        = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.World));
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    return output;
}
#HLSL_END