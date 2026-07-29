#pragma once
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"

/// <summary>
/// 風を発生させるフィールドを作る
/// </summary>
class AccelerationField {
public:
	Vector3 acceleration; // 加速度
	OBB obb; // 範囲

	/// <summary>
	/// 初期化。呼び出すとデフォルトの風を送る。呼びださなくてもいい・
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	/// <param name="particle"></param>
	void Update(Particle particle);
	void Draw();
};