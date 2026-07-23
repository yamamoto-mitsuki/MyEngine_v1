TextureCube<float4> gEnv : register(t0);
SamplerState gSampler : register(s0);

cbuffer Params : register(b0)
{
    float4x4 viewProj; // VSが使う
    float roughness; 　// PSが使う
    float3 pad;
};

struct VSOut
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

// 円周率
static const float PI = 3.14159265359f;

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}
float2 Hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0f * PI * Xi.x;
    float cosT = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinT = sqrt(1.0f - cosT * cosT);
    float3 H = float3(cos(phi) * sinT, sin(phi) * sinT, cosT); // 接空間
    // 接空間 → ワールド
    float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitan = cross(N, tangent);
    return normalize(tangent * H.x + bitan * H.y + N * H.z);
}


float4 main(VSOut input) : SV_TARGET
{
    float3 N = normalize(input.dir);
    float3 R = N;
    float3 V = N;

    const uint SAMPLE_COUNT = 1024u;
    float3 color = float3(0, 0, 0);
    float totalWeight = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f)
        {
            color += gEnv.SampleLevel(gSampler, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    return float4(color / totalWeight, 1.0f);
}