#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 前方宣言
class Camera;

/// <summary>
/// ポイントライト
/// </summary>
class PointLight {
public:
	void Initialize();
	void Update();
	void Draw();

	// ゲッター
	Vector4 GetColor() const { return color_; }
	Vector3 GetPosition() const { return position_; }
	float GetIntensity() const { return intensity_; }
	// セッター
	void SetColor(Vector4 color) { color_ = color; }
	void SetPosition(Vector3 position) { position_ = position; }
	void SetIntensity(float intensity) { intensity_ = intensity; }
	void SetCamera(Camera* camera) { camera_ = camera; }

private:
	Camera* camera_ = nullptr;

	Vector3 position_;
	Vector4 color_;
	float intensity_ = 1.0f;

	PointLightData* mappedPtr_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> lightBuffer_ = nullptr; // GPU転送用バッファ

	Renderer::Rect3dConfig rectConfig_;
	uint32_t textureHandle_;
};