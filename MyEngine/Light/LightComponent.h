#pragma once
#include "MyEngine/Math/Vector3.h"

// Inspectorで編集するライトのデータ
// GPUへ送る構造体（ShaderConstant.h）とは分けて、padding・ポインタ・GPUリソースを持たせない


/// <summary>
/// ライトのエディタ表示のON / OFF。ゲームの絵には影響しない
/// </summary>
struct LightGizmoFlags {
	bool showIcon = true;  // アイコン
	bool showRange = true; // 光の届く範囲のワイヤー
};


/// <summary>
/// 平行光源
/// </summary>
struct DirectionalLightComponent {
	Vector3 color = {1.0f, 1.0f, 1.0f};      // 色
	Vector3 direction = {0.0f, -1.0f, 0.0f}; // 向き
	float intensity = 1.0f;                  // 強さ
};


/// <summary>
/// ポイントライト
/// </summary>
struct PointLightComponent {
	Vector3 position = {0.0f, 0.0f, 0.0f}; // 座標
	Vector3 color = {1.0f, 1.0f, 1.0f};    // 色
	float intensity = 1.0f;                // 強さ
	float radius = 10.0f;                  // ライトの届く最大距離
	float decay = 1.0f;                    // 減衰率（-にはしない）
	LightGizmoFlags gizmo;                 // ギズモの表示設定
};


struct SpotLightComponent {
	static constexpr float kMaxAngle = 89.0f; // 角度の条件（度）。90度で円錐が平らになる

	Vector3 position = {0.0f, 0.0f, 0.0f};   // 座標
	Vector3 direction = {0.0f, -1.0f, 0.0f}; // 照らす向き
	Vector3 color = {1.0f, 1.0f, 1.0f};      // 色
	float intensity = 1.0f;                  // 強さ
	float range = 10.0f;                     // ライトの届く最大距離
	float decay = 1.0f;                      // 減衰率（-にはしない）
	float outerAngle = 30.0f;                // 外側の角度（度）。これより外は照らさない
	float innerAngle = 20.0f;                // 内側の角度（度）。これより内は100%で照らす。外側より大きくしない
	LightGizmoFlags gizmo;                   // ギズモの表示設定
};