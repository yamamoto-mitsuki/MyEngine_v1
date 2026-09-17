#pragma once
#include <cstdint>
#include <type_traits>

#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/Transform.h"

// 描画に必要なデータだけを持つ。処理とGPUリソースは持たない。
struct RenderComponent {
	uint32_t modelHandle = 0;   // 0は未選択（ModelManagerの採番は1から）
	uint32_t textureHandle = 0; // 0ならモデルのマテリアルのテクスチャを使う
	uint32_t color = 0xFFFFFFFF;
	Transform uvTransform;
	MaterialParams material;
	ShadingType shadingType = ShadingType::Unlit;
	BlendMode blendMode = BlendMode::Normal;
	RasterizerType rasterizerType = RasterizerType::SolidBack;
	DepthMode depthMode = DepthMode::TestWrite;
	bool enabled = true;
};

static_assert(std::is_trivially_copyable_v<RenderComponent>);