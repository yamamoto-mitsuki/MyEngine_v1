#pragma once
#include <cstdint>
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
	float GetRadius() const { return radius_; }
	float GetDecay() const { return decay_; }
	// シェーダー転送用データを作成して返す
	PointLightData GetData() const {
		PointLightData data;
		data.color = color_;
		data.position = position_;
		data.intensity = intensity_;
		data.radius = radius_;
		data.decay = decay_;
		return data;
	}
	// セッター 
	void SetColor(const Vector4& color) { color_ = color; }
	void SetPosition(const Vector3& position) { position_ = position; }
	void SetIntensity(float intensity) { intensity_ = intensity; }
	void SetRadius(float radius) { radius_ = radius; }
	void SetDecay(float decay) { decay_ = decay; }
	void SetCamera(Camera* camera) { camera_ = camera; }


private:
	Camera* camera_ = nullptr;

	Vector3 position_ = {0.0f, 0.0f, 0.0f};    // 座標
	Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 色
	float intensity_ = 1.0f; // 強さ
	float radius_ = 10.0f;   // ライトの届く最大距離
	float decay_ = 1.0f;     // 減衰率（-にはしない）
	Renderer::Rect3dConfig rectConfig_;
	uint32_t textureHandle_;
};