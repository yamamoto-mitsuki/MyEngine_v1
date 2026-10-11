#META
stage: Include
path : Resources/TextureCube.hlsli
#META_END

#HLSL
// バインドレスキューブ（IBLのirradiance/prefilter、Skyboxが index で引く）
TextureCube<float32_t4> gTexturesCube[] : register(t0, space1);
#HLSL_END