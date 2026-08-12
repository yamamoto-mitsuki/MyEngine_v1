#include "LightManager.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

void LightManager::Initialize() {
	// --- 画像読み込み ---
	instance_->pointLightTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/PointLight.png");

	// --- GPU系 ---
	instance_->directionlLightBuffer_ = DirectXCommon::CreateMappedUploadBuffer(sizeof(DirectionalLightData),  // DirectionlLight
		reinterpret_cast<void**>(&instance_->directionlLightMappedPtr_));
}