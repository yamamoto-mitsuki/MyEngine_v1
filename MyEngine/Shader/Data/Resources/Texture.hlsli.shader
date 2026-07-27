#META
stage: Include
path : Resources/Texture.hlsli
#META_END

#HLSL
// バインドレス2Dテクスチャ（Material.textureIndex で引く）
Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);
#HLSL_END