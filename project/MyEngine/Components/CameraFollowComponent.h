#pragma once
#include <type_traits>

#include "MyEngine/ECS/Entity.h"
#include "MyEngine/Math/Vector2.h"

// カメラが Entity を追いかける。Camera Component と同じ Entity に付けるComponent（カテゴリ：Camera）
// 横から見るゲーム用：動かすのは X・Y だけ（Z と向きは、置いたときのまま）
// 動かすのは CameraSystem::UpdateFollow（再生中だけ。物理で動いた後・カメラの絵を決める前）。停止中は置いた位置のまま
// 画面の真ん中に置きたい点（focus）＝ 追う物の位置 ＋ Offset ＋ 追い方（Mode）で決まるずれ。カメラは、focus が「遊び」の外へ出た分だけ、なめらかに追う

/// <summary>
/// 追い方（focus をどこに置くか）。Inspector では、選んだ追い方の値だけが出る
/// </summary>
enum class CameraFollowMode {
	Center,    // 追う物を真ん中に（Offset の分だけずらす）
	LookAhead, // 進む向きの先を見せる（追う物の速さ × Look Ahead Time だけ先を真ん中に。止まると真ん中に戻る）
};


/// <summary>
/// カメラの追いかけ（Unity の Cinemachine の、横から見るゲーム用の小さい版）
/// </summary>
struct CameraFollowComponent {
	// --- どの追い方でも使う ---
	bool enabled = true;                                 // false の間は追わない（置いた位置のまま）
	EntityRef target;                                    // 追う Entity（Hierarchy からドラッグ。保存は EntityId で）
	CameraFollowMode mode = CameraFollowMode::LookAhead; // 追い方
	Vector2 offset = {0.0f, 1.5f};                       // 追う物から、画面の真ん中をどれだけずらすか（m。Y を上げると、足元が広く見える）
	Vector2 deadZone = {0.5f, 1.5f};                     // 遊び（m）。画面の真ん中から左右・上下にこの範囲で動いても、カメラは動かない
	bool groundSnapY = true;              // true：追う物が地面にいる間は、上下の遊びを使わずに合わせる（跳んでも画面は揺れず、段に乗ると上がる）。追う物の Rigidbody の isGrounded を見る
	float smoothTime = 0.15f;             // 追いつくまでの時間の目安（秒。0 ならすぐ合わせる）
	Vector2 maxLag = {6.0f, 4.0f};        // 遅れの上限（m）。focus と画面の真ん中が、これより離れない（速く動いても画面から出ないように）
	bool useBounds = false;               // true：画面の端が boundsMin〜boundsMax の外を写さない（ステージの端でカメラが止まる）
	Vector2 boundsMin = {-10.0f, -10.0f}; // 写してよい範囲の左下（ワールドの X・Y。追う物の奥行きで測る）
	Vector2 boundsMax = {100.0f, 30.0f};  // 写してよい範囲の右上
	// --- Look Ahead のときだけ使う ---
	Vector2 lookAheadTime = {0.4f, 0.0f}; // 何秒先を見せるか（先読み ＝ 追う物の速さ × これ。Y を 0 にすると、跳んでも上下に先読みしない）
	Vector2 lookAheadMax = {3.0f, 2.0f};  // 先読みの上限（m）。吹っ飛びのような速い動きで、先へ行きすぎないように
	float lookAheadSmoothTime = 0.3f;     // 先読みが変わるまでの時間の目安（秒。速さが急に変わっても、画面がガクッと動かないように）

	// --- 実行中だけの値（CameraSystem が書く。保存しない）---
	bool isFollowing = false;                  // 追いかけ始めたか（false なら、次のフレームで focus へすぐ合わせる）
	Vector2 currentLookAhead = {0.0f, 0.0f};   // 今の先読み（m。右・上が＋。Center の間は 0 へ戻っていく）
	Vector2 lastTargetPosition = {0.0f, 0.0f}; // 前のフレームの、追う物の位置（速さを出すのに使う）
};

static_assert(std::is_trivially_copyable_v<CameraFollowComponent>);