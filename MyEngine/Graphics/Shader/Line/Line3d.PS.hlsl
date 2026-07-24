#include "Line3d.hlsli"

struct LineMaterial {
    float32_t3 cameraWorldPos;
    float32_t  fadeStartDistance;
    float32_t  fadeEndDistance;
    float32_t  pad0;
    float32_t  pad1;
    float32_t  pad2;
};
ConstantBuffer<LineMaterial> gMaterial : register(b0);


PixelShaderOutput main(VertexShaderOutput input) {
    float32_t dist = length(input.worldPos - gMaterial.cameraWorldPos);
    float32_t fadeFactor = saturate(
        1.0f - (dist - gMaterial.fadeStartDistance)
             / max(gMaterial.fadeEndDistance - gMaterial.fadeStartDistance, 0.0001f)
    );

    PixelShaderOutput output;
    output.color = input.color;
    output.color.a *= fadeFactor;

    if (output.color.a == 0.0f) {
        discard;
    }

    return output;
}