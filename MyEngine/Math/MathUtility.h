#pragma once
#include "MyEngine/Math/Matrix3x3.h"
#include "MyEngine/Math/Matrix4x4.h"
#include "MyEngine/Math/Vector2.h"
#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"
#include "MyEngine/Math/Transform.h"

/// <summary>
/// 数学系ユーティリティ。行列・ベクトル演算をまとめたnamespace。
/// </summary>
namespace MathUtility {

// ===== 行列生成 =====

inline Matrix4x4 MakeIdentity4x4() { return ::MakeIdentity4x4(); }
inline Matrix3x3 MakeIdentity3x3() { return ::MakeIdentity3x3(); }
inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) { return ::MakeTranslateMatrix(translate); }
inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) { return ::MakeScaleMatrix(scale); }
inline Matrix4x4 MakeRotateXMatrix(float radian) { return ::MakeRotateXMatrix(radian); }
inline Matrix4x4 MakeRotateYMatrix(float radian) { return ::MakeRotateYMatrix(radian); }
inline Matrix4x4 MakeRotateZMatrix(float radian) { return ::MakeRotateZMatrix(radian); }
inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) { return ::MakeAffineMatrix(scale, rotate, translate); }
inline Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) { return ::MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip); }
inline Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	return ::MakeOrthographicMatrix(left, top, right, bottom, nearClip, farClip);
}
inline Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) { return ::MakeViewportMatrix(left, top, width, height, minDepth, maxDepth); }
inline Matrix4x4 MakeUVTransformMatrix(const Transform& uvTransform) { return ::MakeUVTransformMatrix(uvTransform); }

// ===== 行列演算 =====
inline Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) { return ::Add(m1, m2); }
inline Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) { return ::Subtract(m1, m2); }
inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) { return ::Multiply(m1, m2); }
inline Matrix4x4 Inverse(const Matrix4x4& m) { return ::Inverse(m); }
inline Matrix4x4 Transpose(const Matrix4x4& m) { return ::Transpose(m); }

// ===== ベクトル変換 =====
inline Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) { return ::TransformNormal(v, m); }

// ===== 汎用 =====

/// 値を min〜max の範囲にクランプする
inline float Clamp(float value, float min, float max) {
	if (value < min) {
		return min;
	}

	if (value > max) {
		return max;
	}

	return value;
}

/// float の線形補間
inline float Lerp(float start, float end, float t) { return start + (end - start) * t; }

/// Vector3 の線形補間
inline Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {
	return {
	    start.x + (end.x - start.x) * t,
	    start.y + (end.y - start.y) * t,
	    start.z + (end.z - start.z) * t,
	};
}

}