#pragma once
#include <vector>

#include "MyEngine/Core/Handle.h"
#include "MyEngine/Core/SlotMap.h"
#include "MyEngine/Light/LightComponent.h"

// 前方宣言
class Camera;


/// <summary>
/// ライトの管理。ライトの実装はここだけが持ち、外にはHandleを渡す。
/// <para>GPUバッファは持たない。GPUへ送るのはRenderContextの役目</para>
/// </summary>
class LightManager {
public:
	static void Initialize();
	static void Release();

	/// <summary>
	/// 更新の最後に呼ぶ。
	/// <para>1. このフレームのライトをGPU用の形にまとめてRendererへ渡す</para>
	/// <para>2. 削除予約されたライトをまとめて消す</para>
	/// </summary>
	static void Update();

	/// <summary>
	/// 全ライトのギズモを描く（エディタ用）
	/// </summary>
	static void DrawGizmos(Camera* camera);

	// ===== 平行光源（1つだけ） =====
	static DirectionalLightComponent* GetDirectionalLight() { return &instance_->directionalLight_; }

	// ===== ポイントライト =====
	static Handle<PointLightComponent> AddPointLight();
	static void RemovePointLight(Handle<PointLightComponent> handle);
	static PointLightComponent* GetPointLight(Handle<PointLightComponent> handle);

	// ===== スポットライト =====
	static Handle<SpotLightComponent> AddSpotLight();
	static void RemoveSpotLight(Handle<SpotLightComponent> handle);
	static SpotLightComponent* GetSpotLight(Handle<SpotLightComponent> handle);

#ifdef USE_IMGUI
	/// <summary>
	/// 確認用のウィンドウ（ライトの追加・削除・値の編集）。Inspectorができるまでの仮
	/// </summary>
	static void DrawDebugWindow();
#endif

private:
	static LightManager* instance_;

	void CollectForGPU(); // Component → GPU用データにまとめてRendererへ渡す
	void FlushRemovals(); // 削除予約を反映する

	DirectionalLightComponent directionalLight_;
	// ポイントライト
	SlotMap<PointLightComponent> pointLights_;
	std::vector<Handle<PointLightComponent>> pendingRemovePointLights_; // 削除予約
	bool hasWarnedPointLightLimit_ = false;                             // 上限越えの警告を1回だけ出す
	// スポットライト
	SlotMap<SpotLightComponent> spotLights_;
	std::vector<Handle<SpotLightComponent>> pendingRemoveSpotLights_; // 削除予約
	bool hasWarnedSpotLightLimit_ = false;                            // 上限越えの警告を1回だけ出す
	// 確認用ウィンドウで追加したライト（メンバ変数はUSE_IMGUIで囲まない）
	std::vector<Handle<PointLightComponent>> debugPointLights_;
	std::vector<Handle<SpotLightComponent>> debugSpotLights_;
};