#include "MyEngine/Light/DirectionalLight.h"
#include "MyEngine/Render/Core/DirectXCommon.h"

void DirectionalLight::Initialize() {
	// GPUバッファ生成
	lightBuffer_ = DirectXCommon::CreateUploadBuffer(sizeof(DirectionalLightData));
	lightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPtr_));
	// 初期値書き込み
	Update();
}

void DirectionalLight::Update() {
	if (mappedPtr_) {
		mappedPtr_->color = color_;
		mappedPtr_->direction = direction_;
		mappedPtr_->intensity = intensity_;
	}
}