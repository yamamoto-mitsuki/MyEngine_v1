#pragma once
#include <cstdint>
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 前方宣言
class Camera;

/// <summary>
/// Editorのグリッド表示クラス
/// </summary>
class EditorGrid {
public:
	void Initialize();
	void Draw();

private:
	uint32_t modelHandle_;
	uint32_t textureHandle_;
	Renderer::ModelConfig model_;
};