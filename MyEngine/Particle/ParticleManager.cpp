#include "MyEngine/Particle/ParticleManager.h"

#include <numbers>
#include <algorithm>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Time/Time.h"
#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Particle/Field/IParticleField.h"
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
// フィールドの登録 / 解除
//=============================================================================
// ===== 登録 =====
void ParticleManager::AddField(IParticleField* field, uint32_t group) {
	MY_ASSERT_MSG(instance_, "Initialize()を先に呼んでください");
	MY_ASSERT_MSG(field, "フィールドがnullptrです");
	MY_ASSERT_MSG(group == kInvalidGroup || group < instance_->groups_.size(), "存在しないグループです");
	// 同じフィールドを二重登録すると効果が倍になるので弾く
	auto& fields = instance_->fields_;
	auto it = std::find_if(fields.begin(), fields.end(), [field](const FieldEntry& e) { return e.field == field; });
	if (it != fields.end()) {
		it->group = group; // 対象だけ差し替える
		return;
	}
	fields.push_back(FieldEntry{field, group});
}

// ===== 解除 =====
void ParticleManager::RemoveField(IParticleField* field) {
	if (!instance_) {
		return;
	}
	auto& fields = instance_->fields_;
	std::erase_if(fields, [field](const FieldEntry& e) { return e.field == field; });
}

// ===== 全解除 =====
void ParticleManager::ClearFields() {
	if (!instance_) {
		return;
	}
	instance_->fields_.clear();
}


//=============================================================================
// 更新
//=============================================================================
void ParticleManager::Update() { 
	auto& inst = *instance_;
	const float deltaTime = Time::GetDeltaTime();

	// ビルボード行列は全粒共通なのでループの外で1回だけ作る
	// カメラのワールド行列 = Viewの逆行列。平行移動を消して回転だけ残す
	// カメラ未設定でも寿命やフィールドは進めたいので、その場合は単位行列で代用する
	Matrix4x4 billboardMatrix = MakeIdentity4x4();
	if (inst.camera_) {
		Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(std::numbers::pi_v<float>);
		billboardMatrix = Multiply(backToFrontMatrix, Inverse(inst.camera_->GetViewMatrix()));
		billboardMatrix.m[3][0] = 0.0f;
		billboardMatrix.m[3][1] = 0.0f;
		billboardMatrix.m[3][2] = 0.0f;
	}

	// ===== ル―プして更新 =====
	for (uint32_t groupIndex = 0; groupIndex < inst.groups_.size(); ++groupIndex) {
		ParticleGroup& group = inst.groups_[groupIndex];
		for (std::list<Particle>::iterator p = group.particles.begin(); p != group.particles.end();) {
			// 時間更新・寿命切れの削除
			p->currentTime += deltaTime;
			if (p->lifeTime <= p->currentTime) {
				p = group.particles.erase(p);
				continue;
			}
			// フィールドの効果（速度への加算など）。座標更新より先に適用して、同フレームで効かせる
			for (const FieldEntry& entry : inst.fields_) {
				if (entry.group == kInvalidGroup || entry.group == groupIndex) {
					entry.field->Apply(*p, deltaTime);
				}
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