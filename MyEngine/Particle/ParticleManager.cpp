#include "MyEngine/Particle/ParticleManager.h"

#include <numbers>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Time/Time.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Graphics/Pipeline/VertexFormat.h"
#include "MyEngine/Graphics/GPU/DirectXCommon.h"

// 静的メンバ変数
ParticleManager* ParticleManager::instance_ = nullptr;


//=============================================================================
// 初期化 / 解放
//=============================================================================
//  ===== 初期化 =====
void ParticleManager::Initialize() { 
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new ParticleManager(); 
	LogManager::Log("Initialized");
}

// ===== 解放 =====
void ParticleManager::Release() {
	if (!instance_) {
		return;
	}

	delete instance_;
	instance_ = nullptr;
	LogManager::Log("Released");
}


//=============================================================================
// エミッターごとのグループを 作成 / 設定変更
//=============================================================================
// ===== 作成 =====
uint32_t ParticleManager::CreateGroup(const ParticleGroupSetting& setting) { 
	MY_ASSERT_MSG(instance_, "Initialize()を先に読んでください"); 
	instance_->groups_.push_back(ParticleGroup{setting, {}});    // エミッターごとの設定を登録
	return static_cast<uint32_t>(instance_->groups_.size() - 1); // このグループの登録番号
}

// ===== 設定変更 =====
void ParticleManager::SetGroupSetting(uint32_t group, const ParticleGroupSetting& setting) { 
	MY_ASSERT_MSG(instance_ && group < instance_->groups_.size(), "存在しないグループです");
	instance_->groups_[group].settings = setting;
}


//=============================================================================
// パーティクルを1個登録
//=============================================================================
void ParticleManager::Register(uint32_t group, const Particle& p) { 
	MY_ASSERT_MSG(instance_, "Initialize()を先に読んでください");
	MY_ASSERT_MSG(group < instance_->groups_.size(), "存在しないグループです");
	auto& particles = instance_->groups_[group].particles;
	// リングバッファ容量の保護。超えた分は登録しない
	if (particles.size() >= kMaxParticles) {
		LogManager::Warning("particles >= kMaxParticles");
		return;
	}
	particles.push_back(p);
}


//=============================================================================
// 更新
//=============================================================================
void ParticleManager::Update() { 
	auto& inst = *instance_;
	const float deltaTime = Time::GetDeltaTime();
	// カメラ未設定のとき
	if (!inst.camera_) {
		LogManager::Warning("Camera Not Found");
		return;
	}

	// ビルボード行列は全粒共通なのでループの外で1回だけ作る
	// カメラのワールド行列 = Viewの逆行列。平行移動を消して回転だけ残す
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
	Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, Inverse(inst.camera_->GetViewMatrix()));
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	// ===== ル―プして更新 =====
	for (ParticleGroup& group : inst.groups_) {
		for (std::list<Particle>::iterator p = group.particles.begin(); p != group.particles.end();) {
			// 時間更新・寿命切れの削除
			p->currentTime += deltaTime;
			if (p->lifeTime <= p->currentTime) {
				p = group.particles.erase(p);
				continue;
			}
			// 座標更新・ビルボード込みのワールド座標を計算（ビルボード行列はループ外で計算済み）
			p->transform.translation += p->velocity * deltaTime;
			p->world = MathUtility::MakeScaleMatrix(p->transform.scale) * billboardMatrix * MathUtility::MakeTranslateMatrix(p->transform.translation);
			p->color.w = 1.0f - (p->currentTime / p->lifeTime);

			++p;
		}
	}
}

//=============================================================================
// 描画
//=============================================================================
void ParticleManager::Draw() { 
	auto& inst = *instance_;
	for (const ParticleGroup& group : inst.groups_) {
		// 空のグループは何も出さない
		if (group.particles.empty()) {
			continue;
		}

		Renderer::ParticleConfig config;
		config.particles = &group.particles;
		config.camera = inst.camera_;
		config.textureHandle = group.settings.textureHandle;
		config.blendMode = group.settings.blendMode;
		config.color = group.settings.color;

		Renderer::DrawParticle(config);
	}
}