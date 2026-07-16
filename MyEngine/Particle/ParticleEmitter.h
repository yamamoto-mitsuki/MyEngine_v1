#pragma once
#include "MyEngine/Math/MathIncludes.h"
#include "MyEngine/Particle/ParticleManager.h"


/// <summary>
/// パーティクルのエミッター
/// </summary>
class ParticleEmitter {
public:
	struct Emitter {
		OBB obb;             // エミッタの範囲
		uint32_t count;      // 発生数
		float frequency;     // 発生頻度
	private:
		float frequencyTime; // 頻度用の時刻
	};

	virtual ~ParticleEmitter() = default;
	virtual void Update() = 0;

	/// <summary>
	/// 設定されたエミッターによってパーティクルを発生させる
	/// </summary>
	/// <param name="emitter">発生させたい設定</param>
	std::list<Particle> Emit(const Emitter& emitter);

	/// <summary>
	/// パーティクルを1つ新しく作成
	/// </summary>
	/// <returns></returns>
	Particle MakeNewParticle();

protected:
	
};