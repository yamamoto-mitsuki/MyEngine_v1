#META
stage: Include
path : Buffers/ObjectTransform.hlsli
#META_END

#HLSL
struct ObjectTransform
{
    float4x4 world;
    float4x4 normalMatrix;
    uint isBillboard;
    float3 padA;
};
ConstantBuffer<ObjectTransform> gObjectTransform : register(b1);
#HLSL_END