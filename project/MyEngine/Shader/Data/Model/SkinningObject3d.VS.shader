#META
stage:   VS
path:    Model/SkinningObject3d.VS.hlsl
profile: vs_6_0
#META_END


#HLSL
#include "Object3d.hlsli"
#include "Buffers/Camera.hlsli"
#include "Buffers/ObjectTransform.hlsli"

// ===== 行列パレット（骨1本につき1つ。番号はモデルのノードの番号）=====
struct Well
{
    float32_t4x4 skeletonSpaceMatrix; // 逆バインド行列 × 今の骨の行列（頂点を曲げる）
    float32_t4x4 skeletonSpaceInverseTransposeMatrix; // 上の行列の逆行列の転置（法線を曲げる）
};
StructuredBuffer<Well> gMatrixPalette : register(t0);

// ===== 頂点の入力（2本の頂点バッファを、同じ頂点番号で1つにまとめた物）=====
struct VertexShaderInput
{
    // --- スロット0：形（Vertex3dData）---
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    // --- スロット1：骨と重み（VertexInfluence）---
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

// ===== 骨で曲げた位置と法線 =====
struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
};

// 骨4本それぞれで動かした位置・法線を、重みで混ぜる
Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    // --- 位置 ---
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f; // 重みの合計が1なので本当は1のまま。計算の誤差を消しておく
    // --- 法線（3x3 で回すだけ。位置の移動は要らない）---
    skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal); // 混ぜると長さが1でなくなるので戻す
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // 骨で曲げてから、いつもどおり Entity のワールド行列で置く（スキンのあるメッシュにノードの行列は掛けない）
    Skinned skinned = Skinning(input);
    float4 worldPos = mul(skinned.position, gObjectTransform.world);
    float3 worldNormal = normalize(mul(skinned.normal, (float32_t3x3) gObjectTransform.normalMatrix));

    output.position = mul(worldPos, gCamera.viewProj);
    output.texcoord = input.texcoord;
    output.normal = worldNormal;
    output.worldPosition = worldPos.xyz;
    return output;
}
#HLSL_END