#include "MyEngine/Particle/ParticleEmitter.h"

#include "MyEngine/Time/Time.h"


//=============================================================================
// 更新
//=============================================================================
void ParticleEmitter::Update() { 
	frequencyTime_ += Time::GetDeltaTime();
	// 発生頻度に応じて、パーティクルを作成
	if (frequencyTime_ >= frequency) {
		frequencyTime_ -= frequency;
		Emit();
	}
}


//=============================================================================
// パーティクルを1つ作成
//=============================================================================
Particle ParticleEmitter::MakeNewParticle() { 
	Particle newParticle;
	newParticle.transform.scale = {1.0f,1.0f,1.0f};
	newParticle.transform.rotation = {0.0f,0.0f,0.0f};
	newParticle.transform.translation = startPosition;
	newParticle.velocity = RandomEngine::GetVector3({-0.5f, -0.5f, -0.5f}, {0.5f,0.5f,0.5f});
	newParticle.lifeTime = RandomEngine::GetFloat(1.0f, 3.0f);
	newParticle.color = {1.0f, 1.0f, 1.0f, 1.0f};
	newParticle.color = RandomEngine::GetVector4({0.0f, 0.0f, 0.0f, 0.0f}, {1.0f,1.0f,1.0f,1.0f});

	return newParticle;
}


//=============================================================================
// グループの取得（未作成なら作成）
//=============================================================================
uint32_t ParticleEmitter::GetGroup() {
	if (group_ == ParticleManager::kInvalidGroup) {
		group_ = ParticleManager::CreateGroup(setting);
	}
	return group_;
}


//=============================================================================
// 設定したエミッターによってパーティクルを発生
//=============================================================================
void ParticleEmitter::Emit() {
	// 初回はここでグループが作られる。2回目以降は設定を反映するだけ
	uint32_t group = GetGroup();
	ParticleManager::SetGroupSetting(group, setting);
	// 登録
	for (uint32_t i = 0; i < count; ++i) {
		ParticleManager::Register(group, MakeNewParticle());
	}
}