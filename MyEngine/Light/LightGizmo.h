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
	/// 積んだ線を1回で描く
	/// </summary>
	static void End();

	// 全体の表示設定。ラ内ごとの設定とANDで判定
	static LightGizmoFlags& GetGlobalFlags() { return globalFlags_; }

private:
	static uint32_t iconTextureHandle_;
	static LightGizmoFlags globalFlags_;
	static Camera* camera_;                      // Begin～Endの間だけ使う。フレームをまたがない
	static bool isRecording_;                    // Begin～Endの間ならtrue
	static Renderer::LineListConfig rangeLines_; // 全ライト分の線の置き場。配列のメモリを使い回す
};