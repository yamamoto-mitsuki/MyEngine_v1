#include "Sprite2d.hlsli"

struct WindowSize {
    float32_t width;
    float32_t height;
    float32_t2 padding;
};
ConstantBuffer<WindowSize> gWindowSize : register(b0);

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;

    float32_t ndcX =  (input.position.x / gWindowSize.width)  * 2.0f - 1.0f;
    float32_t ndcY = -((input.position.y / gWindowSize.height) * 2.0f - 1.0f);

    output.position = float32_t4(ndcX, ndcY, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}