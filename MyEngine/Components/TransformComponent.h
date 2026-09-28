#pragma once
#include "MyEngine/Math/MathIncludes.h"

// Inspectorで編集するEntityの位置・回転・大きさ
// ARCHITECTURE.md 原則2：ポインタ・可変長の型・仮想関数・GPUリソースを持たない


/// <summary>
/// 位置・回転・大きさ
/// <para>worldMatrix は EntityManager::UpdateTransforms が毎フレーム計算する（親 → 子の順）</para>
/// </summary>
struct TransformComponent {
	Vector3 translation = {0.0f, 0.0f, 0.0f};  // 位置（親から見た位置）
	Vector3 rotation = {0.0f, 0.0f, 0.0f};     // 回転（ラジアン。X→Y→Zの順）
	Vector3 scale = {1.0f, 1.0f, 1.0f};        // 大きさ
	Matrix4x4 worldMatrix = MakeIdentity4x4(); // 計算結果（親の行列まで掛けたもの）
};

// ===== worldMatrix から読み取る（UpdateTransformsの後の値。作った直後のフレームは原点・回転なし）=====

// ワールド座標（行列の4行目＝平行移動）
inline Vector3 GetWorldPosition(const TransformComponent& transform) {
	const Matrix4x4& m = transform.worldMatrix;
	return {m.m[3][0], m.m[3][1], m.m[3][2]};
}

// 前（ローカルの+Z）がワールドでどちらを向いているか。長さ1（大きさが0なら(0,0,0)）
// 行列の3行目＝ローカルの(0,0,1)を変換した向き。ライトはこの向きに照らす
inline Vector3 GetWorldForward(const TransformComponent& transform) {
	const Matrix4x4& m = transform.worldMatrix;
	return Normalize(Vector3{m.m[2][0], m.m[2][1], m.m[2][2]});
}