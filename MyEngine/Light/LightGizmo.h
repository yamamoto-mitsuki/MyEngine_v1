#pragma once
#include <cstdint>

#include "MyEngine/Light/LightComponent.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 前方宣言
class Camera;


/// <summary>
/// ライトをエディタ上で見せるための表示用のギズモ
/// </summary>
class LightGizmo {
public:
	static void Initialize();

	/// <summary>
	/// 1回分の表示を始める。前回積んだ線を捨てる。
	/// </summary>
	/// <param name="camera"></param>
	static void Begin(Camera* camera);

	/// <summary>
	/// ポイントライトを1つ積む。アイコンはその場で描き、線はEndでまとめて描く。
	/// </summary>
	/// <param name="light"></param>
	static void AddPointLight(const PointLightComponent& light);

	/// <summary>
	/// スポットライトを1つ積む。範囲は円錐（底の円 ＋ 頂点から円への4本の線）で表す
	/// </summary>
	static void AddSpotLight(const SpotLightComponent& light);

	/// <summary>
	/// 積んだ線を1回で描く
	/// </summary>
	static void End();

    // 全体の表示設定。ライトごとの設定とANDで判定
	static LightGizmoFlags& GetGlobalFlags() { return globalFlags_; }


private:
	// アイコンを1つ描く（ポイントライトとスポットライトで共通。絵だけ差し替える）
	static void DrawIcon(const Vector3& position, uint32_t textureHandle);

	static uint32_t pointIconTextureHandle_; // ポイントライトのアイコン（電球）
	static uint32_t spotIconTextureHandle_;  // スポットライトのアイコン（懐中電灯）
	static LightGizmoFlags globalFlags_;
	static Camera* camera_;                      // Begin～Endの間だけ使う。フレームをまたがない
	static bool isRecording_;                    // Begin～Endの間ならtrue
	static Renderer::LineListConfig rangeLines_; // 全ライト分の線の置き場。配列のメモリを使い回す
};