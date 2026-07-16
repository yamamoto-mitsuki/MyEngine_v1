struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
    uint32_t textureIndex : TEXTUREINDEX0;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};