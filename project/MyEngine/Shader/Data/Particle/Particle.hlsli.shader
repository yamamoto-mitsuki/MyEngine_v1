#META
stage: Include
path:  Particle/Particle.hlsli
#META_END

#HLSL
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};
#HLSL_END