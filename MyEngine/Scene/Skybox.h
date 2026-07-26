#pragma once
#include <cstdint>
#include <memory>
#include <wrl.h>
#include <d3d12.h>
#include "MyEngine/Graphics/RenderTarget/RenderTextureCube.h"
#include "MyEngine/Graphics/Pipeline/RootSignatureManager.h"

// 前方宣言
class Camera;


/// <summary>
/// 天球（キューブマップ方式）。.png / .hdrどちらでも
/// </summary>
class Skybox {
public:
	void Initialize();
	void Draw(const Camera* camera);

	// セッター
	void SetEquirect(uint32_t handle);
	void SetIntensity(float v) { intensity_ = v; }
	void SetRotationY(float rad) { rotationY_ = rad; }

private:
	void CreateRootSignature();
	void CreatePSO();

	RootSignatureInfo rsInfo_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cb_;
	uint8_t* cbMapped_ = nullptr;

	std::unique_ptr<RenderTextureCube> ownedCube_; // 使用キューブ
	RenderTextureCube* cube_ = nullptr;
	float intensity_ = 1.0f;
	float rotationY_ = 0.0f;
};