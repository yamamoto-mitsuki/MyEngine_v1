#META
stage: Include
path : Buffers/Light.hlsli
#META_END

#HLSL
// 平行光源
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

// ポイントライト
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float padA;
    float padB;
};

static const int kMaxPointLights = 16; // C++ kMaxPointLights と一致させること
struct PointLightLists
{
    PointLight lights[kMaxPointLights];
    uint count;
    float padA;
    float padB;
    float padC;
};
ConstantBuffer<PointLightLists> gPointLights : register(b3);
#HLSL_END