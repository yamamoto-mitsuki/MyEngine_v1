#META
stage: Include
path : Buffers/ObjectTransform.hlsli
#META_END

#HLSL
struct ObjectTransform
{
    float4x4 world;
};
ConstantBuffer<ObjectTransform> gObjectTransform : register(b1);
#HLSL_END