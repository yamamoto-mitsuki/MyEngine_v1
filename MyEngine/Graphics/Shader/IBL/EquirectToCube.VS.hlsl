cbuffer Camera : register(b0)
{
    float4x4 viewProj;
};

struct VertexOutput
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

VertexOutput main(float3 pos : POSITION)
{
    VertexOutput output;
    output.dir = pos;
    output.pos = mul(float4(pos, 1.0f), viewProj);
    return output;
}