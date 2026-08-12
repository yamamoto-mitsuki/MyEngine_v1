#pragma once
#include <list>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "MyEngine/Light/LightIncludes.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"

class LightManager {
public:
	static void Initialize();
	static void Release();
	static void Update();
	static void Draw();

	static PointLight* AddPointLight();

private:
	static LightManager* instance_;
	// ライト本体
	DirectionalLight* directionlLight = nullptr;
	std::list<PointLight*> pointLights_;
	uint32_t pointLightTextureHandle_ = 0u;

	// GPU関係
	Microsoft::WRL::ComPtr<ID3D12Resource> directionlLightBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightBuffer_;
	DirectionalLightData* directionlLightMappedPtr_ = nullptr;
	PointLightData* pointLightMappedPtr_ = nullptr;
};