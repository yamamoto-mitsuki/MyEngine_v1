#include "Line3d.hlsli"

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t4 color    : COLOR0;
};

struct TransformationMatrix {
    float4x4 wvpMatrix;
    float4x4 worldMatrix;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.wvpMatrix);
    output.worldPos = mul(input.position, gTransformationMatrix.worldMatrix).xyz;
    output.color = input.color;
    return output;
}
