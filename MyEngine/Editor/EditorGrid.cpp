#include "EditorGrid.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

void EditorGrid::Initialize() { 
	modelHandle_ = ModelManager::Load("MyEngine/Resources/Model/grid.obj");
	textureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/grid.png");
	model_.modelHandle = modelHandle_;
	model_.textureHandle = textureHandle_;
	model_.shadingType = ShadingType::Unlit;
	model_.rasterizerType = RasterizerType::SolidNone;
}

void EditorGrid::Draw() { 
	Renderer::DrawModel(model_);
}