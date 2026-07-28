#pragma once
#include <cstdint>
#include "MyEngine/Graphics/Renderer/Renderer.h"

class EditorGrid {
public:
	void Initialize();
	void Update();
	void Draw();

private:
	uint32_t modelHandle_;
	Renderer::ModelConfig model_;
};