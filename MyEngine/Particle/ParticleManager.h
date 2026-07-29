#pragma once
#include <list>

#include <d3d12.h>
#include <wrl.h>

#include "MyEngine/Graphics/Pipeline/ShaderConstants.h"
#include "MyEngine/Graphics/Pipeline/RenderStates.h"
#include "MyEngine/Math/MathIncludes.h"

// 前方宣言
class Camera;

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

	// セッター
	static void SetCamera(Camera* camera) { instance_->camera_ = camera; }

private:
	// グループ
	struct ParticleGroup {
		ParticleGroupSetting settings;
		std::list<Particle> particles;
	};

	// インスタンス
	static ParticleManager* instance_;

	Camera* camera_; // カメラ
	std::vector<ParticleGroup> groups_;
};