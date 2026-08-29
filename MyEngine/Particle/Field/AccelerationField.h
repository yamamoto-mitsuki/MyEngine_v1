#pragma once
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"
#include "MyEngine/Particle/Field/IParticleField.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

/// <summary>
/// 風を発生させるフィールドを作る
/// <para>ParticleManager::AddField() で登録すると、範囲内のパーティクルが加速する</para>
/// </summary>
class AccelerationField : public IParticleField {
public:
	Vector3 acceleration = {15.0f, 0.0f, 0.0f};              // 加速度（1秒あたりに速度へ足す量）
	AABB aabb = {{-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f}}; // 範囲（ワールド座標のmin/max）
	bool isActive = true;                                    // falseで効果を止める（描画は残る）

	/// <summary>
	/// 初期化。既定値に戻して描画設定を作る
	/// </summary>
	void Initialize();

	/// <summary>
	/// 範囲内のパーティクルを加速させる。ParticleManagerから毎フレーム呼ばれる
	/// </summary>
	/// <param name="particle">対象のパーティクル</param>
	/// <param name="deltaTime">経過時間（秒）</param>
	void Apply(Particle& particle, float deltaTime) override;

	/// <summary>
	/// 描画。ワイヤーフレームで行う。
	/// </summary>
	void Draw();

	/// <summary>描画に使うカメラ。半透明のソートに使われる</summary>
	void SetCamera(Camera* camera) { drawConfig_.camera = camera; }

	/// <summary>中心と半径サイズから範囲を作る</summary>
	void SetBox(const Vector3& center, const Vector3& halfSize) {
		aabb.min = center - halfSize;
		aabb.max = center + halfSize;
	}

private:
	Renderer::AABBConfig drawConfig_; // 描画用
};
