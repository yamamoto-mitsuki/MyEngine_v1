#META
stage: Include
path:  Sprite/Sprite2d.hlsli
#META_END

#HLSL
struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
#HLSL_END