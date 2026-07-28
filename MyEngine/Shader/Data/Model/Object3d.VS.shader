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

    float4 worldPos;
    float3 worldNormal;
    if (gObjectTransform.isBillboard != 0)
    {
        // world から中心とスケールを取り出す
        float3 center = float3(gObjectTransform.world._41, gObjectTransform.world._42, gObjectTransform.world._43);
        float sx = length(float3(gObjectTransform.world._11, gObjectTransform.world._12, gObjectTransform.world._13));
        float sy = length(float3(gObjectTransform.world._21, gObjectTransform.world._22, gObjectTransform.world._23));
        // カメラ基底で板を組む（常にカメラを向く）
        float3 p = center + gCamera.right * (input.position.x * sx)
                          + gCamera.up * (input.position.y * sy);
        worldPos = float4(p, 1.0f);
        worldNormal = normalize(cross(gCamera.right, gCamera.up)); // 板の法線＝カメラ方向
    }
    else
    {
        worldPos = mul(input.position, gObjectTransform.world);
        worldNormal = normalize(mul(input.normal, (float32_t3x3) gObjectTransform.world));
    }

    output.position = mul(worldPos, gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.normal = worldNormal;
    output.worldPosition = worldPos.xyz;
    return output;
}
#HLSL_END