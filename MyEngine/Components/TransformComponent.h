#pragma once
#include <cmath>

#include "MyEngine/Math/MathIncludes.h"

// Inspectorで編集するEntityの位置・回転・大きさ


/// <summary>
/// 位置・回転・大きさ
/// <para>worldMatrix は EntityManager::UpdateTransforms が毎フレーム計算する（親 → 子の順）</para>
/// <para>回転は中ではクォータニオンで持つ（Unity と同じ）。角度（度）で書きたいときは SetEulerAngles・Rotate を使う</para>
/// </summary>
struct TransformComponent {
	Vector3 translation = {0.0f, 0.0f, 0.0f};       // 位置（親から見た位置）
	Quaternion rotation = {0.0f, 0.0f, 0.0f, 1.0f}; // 回転（親から見た向き。長さ1のクォータニオン）
	Vector3 scale = {1.0f, 1.0f, 1.0f};             // 大きさ
	Vector3 eulerHint = {0.0f, 0.0f, 0.0f};         // エディタ用：Inspector に出すオイラー角（度）の元。ゲームからは書かない（Unity の m_LocalEulerAnglesHint と同じ）
	Matrix4x4 worldMatrix = MakeIdentity4x4();      // 計算結果（親の行列まで掛けたもの）
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

// ===== 回転を角度（度）で読み書きする（rotation はクォータニオンなので、角度で書きたいときはこれ）=====
// オイラー角は Unity と同じ「度」で、Z → X → Y の順に回す（Inspector の Rotation と同じ）

// オイラー角（度）で回転を決める。Inspector にもこの値がそのまま出る（Unity の transform.localEulerAngles = … と同じ）
// 例：SetEulerAngles(transform, {0.0f, 90.0f, 0.0f}); // 右（+X）を向く
inline void SetEulerAngles(TransformComponent& transform, const Vector3& euler) {
	transform.rotation = MakeQuaternionFromEuler(euler);
	transform.eulerHint = euler;
}

// 今の回転をオイラー角（度）で。Inspector と同じ値（同じ向きの表し方のうち、最後に決めた角度に一番近い物）
// sin・cos の計算が要るので、毎フレームたくさんの物で呼ぶ所では、rotation（クォータニオン）のまま計算する
inline Vector3 GetEulerAngles(const TransformComponent& transform) {
	constexpr float kSameRotation = 1.0f - 1e-6f; // 2つのクォータニオンの内積の大きさがこれ以上なら、同じ向きとみなす
	// eulerHint が今の向きのままなら、そのまま返す（人が打った値・SetEulerAngles で決めた値が、そのまま出る）
	if (std::abs(Dot(MakeQuaternionFromEuler(transform.eulerHint), transform.rotation)) >= kSameRotation) {
		return transform.eulerHint;
	}
	// コードで rotation を書き換えた後：eulerHint に一番近い表し方（数字が 180 度飛んだりしない）
	return ToEulerNearest(transform.rotation, transform.eulerHint);
}

// 今の向きから、さらに回す（物の軸のまわり。Unity の transform.Rotate と同じ）。euler はオイラー角（度。Z → X → Y の順）
// 例：Rotate(transform, speed * Time::GetDeltaTime()); // speed（度/秒）を X・Y・Z ごとに
inline void Rotate(TransformComponent& transform, const Vector3& euler) {
	// 右に掛けた方が先に回る（Quaternion.h の operator*）。だから、今の向きにする前の「物の軸」のまわりに回る
	// 掛け算を続けると長さが1から少しずつずれていくので、毎回1に戻す
	transform.rotation = Normalize(transform.rotation * MakeQuaternionFromEuler(euler));
}

// 物の軸 axis のまわりに angle（度）回す。1つの軸で回すなら、こちらの方が計算が少ない（sin・cos が1組だけ）
// 例：Rotate(transform, {0.0f, 1.0f, 0.0f}, spinSpeed * Time::GetDeltaTime()); // 毎秒 spinSpeed 度、Y 軸のまわりに
// たくさんの物を同じだけ回すなら、回す量 delta を1回だけ作って transform.rotation = Normalize(transform.rotation * delta); と書くと、もっと速い
inline void Rotate(TransformComponent& transform, const Vector3& axis, float angle) { transform.rotation = Normalize(transform.rotation * MakeRotateAxisAngleQuaternion(axis, angle)); }