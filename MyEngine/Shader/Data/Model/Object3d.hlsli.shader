#META
stage: Include
path:  Model/Object3d.hlsli
#META_END

#HLSL
struct VertexShaderOutput {
    float4 position      : SV_POSITION;
    float2 texcoord      : TEXCOORD0;
    float3 normal        : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
#HLSL_END