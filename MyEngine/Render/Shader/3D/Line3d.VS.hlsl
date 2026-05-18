#include "Line3d.hlsli"

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t4 color    : COLOR0;
};

struct TransformationMatrix {
    float32_t4x4 wvpMatrix;
    float32_t4x4 worldMatrix;
};
ConstantBuffer<TransformationMatrix> gTransform : register(b0);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransform.wvpMatrix);
    output.worldPos = mul(input.position, gTransform.worldMatrix).xyz;
    output.color = input.color;
    return output;
}
