#pragma once
#include "MyEngine/Math/Vector3.h"

// 当たり判定の形と大きさ。Entityに付けるComponent（カテゴリ：Physics）
// 位置は持たない。付けたEntityのTransformから取る（ライトと同じ）
// 重なったかどうかを CollisionSystem が調べて、ゲームは Script の OnTrigger〜 / OnCollision〜、または CollisionSystem::GetHits で読む
// 押し戻す（ぶつかったら止まる）のは、Rigidbody を付けた物だけ（PhysicsSystem）。Is Trigger の Collider は押し戻さない（すり抜ける）

/// <summary>
/// 当たり判定の形
/// </summary>
enum class ColliderShape {
	Sphere,  // 球。中心と半径
	Box,     // 箱。中心と大きさ。Entityを回しても箱は回らない（軸にそろった箱＝AABB）
	Capsule, // カプセル（縦の線分を半径で太らせた形。円柱の上下に半球）。中心・半径・高さ。Entityを回してもいつも立ったまま（プレイヤー向け）
};

/// <summary>
/// 当たり判定
/// </summary>
struct ColliderComponent {
	bool enabled = true;                      // falseの間は当たらない（外さずに、一時的に止めたいとき）
	bool isTrigger = false;                   // true：すり抜ける（Rigidbody を押し戻さない。Script には OnTrigger〜）。false：ぶつかる（Script には OnCollision〜）
	ColliderShape shape = ColliderShape::Box; // 形
	Vector3 center = {0.0f, 0.0f, 0.0f};      // Entityの位置からのずれ（Entityのローカル座標。Entityの回転と大きさも掛かる）
	float radius = 0.5f;                      // 球・カプセルの半径（球は Entity の大きさの一番大きい軸を、カプセルは X と Z の大きい方を掛ける）
	float height = 2.0f;                      // カプセルの高さ（下の端から上の端まで。半球も入れる）。Entity の Y の大きさを掛ける。半径の2倍より小さいと球になる
	Vector3 size = {1.0f, 1.0f, 1.0f};        // 箱の幅・高さ・奥行き（Entityの大きさを軸ごとに掛ける）
};