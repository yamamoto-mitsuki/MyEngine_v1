#META
stage: Include
path : Buffers/TransformationMatrix.hlsli
#META_END

#HLSL
struct TransformationMatrix
{
    float4x4 wvpMatrix;
    float4x4 worldMatrix;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
#HLSL_END