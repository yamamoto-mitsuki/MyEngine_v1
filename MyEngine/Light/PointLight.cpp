#include "MyEngine/Light/PointLight.h"

#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"


void PointLight::Initialize() {
	// 描画設定
	textureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/PointLight.png");
	rectConfig_.textureHandle = textureHandle_;
	rectConfig_.shadingType = ShadingType::Unlit;
	rectConfig_.blendMode = BlendMode::Normal;
	rectConfig_.rasterizerType = RasterizerType::SolidNone;
	rectConfig_.billboard = true;
	//rectConfig_.transform.scale = {0.5f, 0.5f, 0.5f};
}


void PointLight::Update() {
	rectConfig_.camera = camera_; // 描画時のカメラ更新
	rectConfig_.transform.translation = position_;
}


void PointLight::Draw() { Renderer::DrawRect3d(rectConfig_); }