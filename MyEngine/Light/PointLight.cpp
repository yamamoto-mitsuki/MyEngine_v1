#include "PointLight.h"

#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"


void PointLight::Initialize() {
	// GPUバッファ生成
	lightBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(PointLightData), reinterpret_cast<void**>(&mappedPtr_));
	lightBuffer_->SetName(L"PointLight_CB");
	// 描画設定
	textureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/PointLight.png");
	rectConfig_.textureHandle = textureHandle_;
	rectConfig_.shadingType = ShadingType::Unlit;
	// 初期値書き込み
	Update();
}


void PointLight::Update() {
	if (mappedPtr_) {
		mappedPtr_->position = position_;
		mappedPtr_->color = color_;
		mappedPtr_->intensity = intensity_;
	}

	rectConfig_.camera = camera_; // 描画時のカメラ更新
	rectConfig_.transform.translation = position_;
}


void PointLight::Draw() { Renderer::DrawRect3d(rectConfig_); }