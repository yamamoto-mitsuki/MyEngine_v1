#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Render/Core/ShaderStructs.h"

class DirectionalLight {
public:
	void Initialize();
	void Update();

	// ゲッター
	Vector4 GetColor() const { return color_; }
	Vector3 GetDirection() const { return direction_; }
	float GetIntensity() const { return intensity_; }
	ID3D12Resource* GetBuffer() const { return lightBuffer_.Get(); }
	// セッター
	void SetColor(const Vector4& color) { color_ = color; }
	void SetDirection(const Vector3& direction) { direction_ = direction; }
	void SetIntensity(float intensity) { intensity_ = intensity; }

private:
	Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 direction_ = {0.0f, -1.0f, 0.0f};
	float intensity_ = 1.0f;

	DirectionalLightData* mappedPtr_ = nullptr;

	// GPU送信用バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> lightBuffer_ = nullptr;
};
