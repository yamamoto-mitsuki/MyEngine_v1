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