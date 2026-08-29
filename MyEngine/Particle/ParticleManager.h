#pragma once
#include <list>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/MathIncludes.h"

// 前方宣言
class Camera;
class IParticleField;

// 1個分のデータ
struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime = 0.0f;
	float currentTime = 0.0f;
	Matrix4x4 world; // ビルボード込みのワールド行列
	bool isBillboard = true; // ビルボード
};

// エミッターごとに設定する項目
struct ParticleGroupSetting {
	uint32_t textureHandle = 0; // 使用するTexture
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 全体の色
	BlendMode blendMode = BlendMode::Add; // ブレンドモード
};

/// <summary>
/// バッファ / SRV / 描画を持つシングルトン
/// </summary>
class ParticleManager {
public:
	static constexpr uint32_t kMaxParticles = 4096;// 1グループの最大数
	static constexpr uint32_t kInvalidGroup = 0xFFFFFFFF; // 無効なグループハンドル

	static void Initialize();
	static void Release();
	static void Update();
	static void Draw();

	/// <summary>
	/// グループ（エミッター単位の描画設定 + パーティクルリスト）を作成し、ハンドルを返す
	/// </summary>
	static uint32_t CreateGroup(const ParticleGroupSetting& setting);

	/// <summary>
	/// グループの描画設定を変更する（エミッターで変更した値を反映したいとき）
	/// </summary>
	/// <param name="group"></param>
	/// <param name="setting"></param>
	static void SetGroupSetting(uint32_t group, const ParticleGroupSetting& setting);

	/// <summary>
	/// パーティクルを1個グループに登録する
	/// </summary>
	static void Register(uint32_t group, const Particle& p);

	/// <summary>全グループの粒を消す（シーン切り替え時に呼ぶ）</summary>
	static void ClearParticles() {
		for (ParticleGroup& group : instance_->groups_) {
			group.particles.clear();
		}
	}

	/// <summary>
	/// フィールド（加速や引力などの効果）を登録する。
	/// <para>Update()の座標更新直前に、生存中のパーティクルへ Apply() が呼ばれる</para>
	/// <para>フィールドの実体はゲーム側が持つ。破棄する前に必ず RemoveField() すること</para>
	/// </summary>
	/// <param name="field">効果の実体。IParticleFieldを継承したクラス</param>
	/// <param name="group">効かせるグループ。kInvalidGroupで全グループが対象</param>
	static void AddField(IParticleField* field, uint32_t group = kInvalidGroup);

	/// <summary>登録したフィールドを外す。登録されていなければ何もしない</summary>
	static void RemoveField(IParticleField* field);

	/// <summary>全フィールドを外す（シーン切り替え時に呼ぶ）</summary>
	static void ClearFields();

	// セッター
	static void SetCamera(Camera* camera) { instance_->camera_ = camera; }

private:
	// グループ
	struct ParticleGroup {
		ParticleGroupSetting settings;
		std::list<Particle> particles;
	};

	// 登録されたフィールドと、効かせる対象
	struct FieldEntry {
		IParticleField* field = nullptr;
		uint32_t group = kInvalidGroup; // kInvalidGroupなら全グループ
	};

	// インスタンス
	static ParticleManager* instance_;

	Camera* camera_ = nullptr; // カメラ
	std::vector<ParticleGroup> groups_;
	std::vector<FieldEntry> fields_;
};