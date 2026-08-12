#pragma once
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

/// <summary>
/// 風を発生させるフィールドを作る
/// </summary>
class AccelerationField {
public:
	Vector3 acceleration; // 加速度
	AABB aabb;            // 範囲

	/// <summary>
	/// 初期
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新。パーティクルがフィールドの中に入っているか判定し、速度などを適応
	/// </summary>
	void Update(Particle* particle);

	/// <summary>
	/// 描画。ワイヤーフレームで行う。
	/// </summary>
	void Draw();

private:
	void RegisterGV();
	void ApplyGV();

	Renderer::AABBConfig drawConfig_; // 描画用
};