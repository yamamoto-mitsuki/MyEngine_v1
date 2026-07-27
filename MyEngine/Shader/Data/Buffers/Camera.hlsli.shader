#META
stage: Include
path : Buffers/Camera.hlsli
#META_END

#HLSL
struct Camera
{
    float4x4 viewProj;        // ビュー×プロジェクション
    float32_t3 worldPosition; // カメラのワールド座標（鏡面反射用）
    float padA;
    float32_t3 right;         // カメラ右方向（ビルボード用）
    float padB;
    float32_t3 up;            // カメラ上方向（ビルボード用）
    float padC;
};
ConstantBuffer<Camera> gCamera : register(b2);
#HLSL_END