#pragma once

// 前方宣言
struct Particle;

/// <summary>
/// パーティクルに力や色などの効果を与えるフィールドの基底
/// <para>ParticleManager::AddField() で登録すると、毎フレームの座標更新の直前に Apply() が呼ばれる</para>
/// </summary>
class IParticleField {
public:
	virtual ~IParticleField() = default;

	/// <summary>
	/// パーティクル1個への効果を適用する。範囲判定もこの中で行う
	/// </summary>
	/// <param name="particle">対象のパーティクル（生存中のものだけが渡る）</param>
	/// <param name="deltaTime">経過時間（秒）</param>
	virtual void Apply(Particle& particle, float deltaTime) = 0;
};
