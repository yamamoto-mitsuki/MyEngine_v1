#pragma once
#include <type_traits>

#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Vector3.h"

// カメラに注視点を見させる。Camera Component と同じ Entity に付けるComponent（カテゴリ：Camera）
// 変えるのは写す向きだけで、Transform の向きは変えない（Camera Look At を外すと、置いた向きに戻る）
// 停止中も見る向きが Game ビューに出る。なめらかに動くのは再生中だけ

/// <summary>
/// 注視点（Unity の Cinemachine の Rotation Composer の小さい版）
/// </summary>
struct CameraLookAtComponent {
	bool enabled = true;                 // false の間は Transform の向きのまま
	EntityRef target;                    // 見る Entity（Hierarchy からドラッグ）。空なら Offset をワールドの座標として見る
	Vector3 offset = {0.0f, 0.0f, 0.0f}; // 見る点のずれ（m。ワールドの向き）
	float smoothTime = 0.0f;             // 見る点が追いつくまでの時間の目安（秒。0 ならすぐ合わせる）

	// --- 実行中だけの値（CameraSystem が書く。保存しない）---
	bool isAiming = false;                     // 見始めたか（false なら、次のフレームで見る点へすぐ合わせる）
	Vector3 currentPoint = {0.0f, 0.0f, 0.0f}; // 今見ている点（ワールド）
};

static_assert(std::is_trivially_copyable_v<CameraLookAtComponent>);