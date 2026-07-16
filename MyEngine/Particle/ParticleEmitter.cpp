#include "MyEngine/Particle/ParticleEmitter.h"


//=============================================================================
// パーティクルを1つ作成
//=============================================================================
Particle ParticleEmitter::MakeNewParticle(const Emitter& emitter) { 
	Particle newParticle;
	newParticle.transform.scale = {1.0f,1.0f,1.0f};
	newParticle.transform.rotation = {0.0f,0.0f,0.0f};
	newParticle.transform.translation = emitter.startPosition;
	newParticle.velocity = RandomEngine::GetVector3({-0.5f, -0.5f, -0.5f}, {0.5f,0.5f,0.5f});
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