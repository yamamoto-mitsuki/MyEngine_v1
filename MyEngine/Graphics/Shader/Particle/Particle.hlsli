struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION1;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};