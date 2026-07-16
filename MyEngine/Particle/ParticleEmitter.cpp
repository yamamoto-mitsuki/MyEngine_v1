#include "ParticleEmitter.h"


//=============================================================================
// パーティクルを1つ作成
//=============================================================================
Particle ParticleEmitter::MakeNewParticle() { 
	Particle newParticle;
	newParticle.transform.scale;
	newParticle.transform.rotation;
	newParticle.transform.translation; // エミッターのOBBの中心から
	newParticle.velocity; // ランダム+Field
	newParticle.lifeTime;
}

//=============================================================================
// 設定したエミッターによってパーティクルを発生
//=============================================================================
std::list<Particle> ParticleEmitter::Emit(const Emitter& emitter) { 
	std::list<Particle> particles; 

	for (uint32_t i = 0; i < emitter.count; ++i) {
		MakeNewParticle();
	}
	return particles;
}