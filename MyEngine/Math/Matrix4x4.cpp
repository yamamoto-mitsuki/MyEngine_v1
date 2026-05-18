#include "MyEngine/Math/Matrix4x4.h"
#include <algorithm>
#include <cassert>
#include <cmath>

// ===== 二項演算子 =====

// 行列の加算
Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2) { return Add(m1, m2); }
// 行列の減算
Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2) { return Subtract(m1, m2); }
// 行列の乗算
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2) { return Multiply(m1, m2); }

// ===== 単項演算子 =====
Matrix4x4 operator-(const Matrix4x4& m) {
	Matrix4x4 result = MakeIdentity4x4();
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = -m.m[i][j];
		}
	}
	return result;
}

// ===== 行列の加法 =====
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = MakeIdentity4x4();
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = m1.m[i][j] + m2.m[i][j];
		}
	}
	return result;
}

// ===== 行列の減法 =====
Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = MakeIdentity4x4();
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = m1.m[i][j] - m2.m[i][j];
		}
	}
	return result;
}
// ===== 行列の積 =====
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = m1.m[i][0] * m2.m[0][j] + m1.m[i][1] * m2.m[1][j] + m1.m[i][2] * m2.m[2][j] + m1.m[i][3] * m2.m[3][j];
		}
	}
	return result;
}

// ===== 逆行列 =====
Matrix4x4 Inverse(const Matrix4x4& m) {
	Matrix4x4 inv = MakeIdentity4x4();
	Matrix4x4 temp = m;

	for (int i = 0; i < 4; i++) {
		// --- ピボット選択 ---
		// その列の中で一番絶対値が大きい数字を持つ行を探し、現在の行と入れ替える
		int pivot = i;
		for (int j = i + 1; j < 4; j++) {
			if (std::abs(temp.m[j][i]) > std::abs(temp.m[pivot][i]))
				pivot = j;
		}
		std::swap(temp.m[i], temp.m[pivot]);
		std::swap(inv.m[i], inv.m[pivot]);
		// 逆行列が存在しないとき
		assert(std::abs(temp.m[i][i]) > 1e-6f && "逆行列が存在しません。");

		// --- 正規化 ---
		// 現在の行のすべての要素を対角成分で除算
		float div = temp.m[i][i];
		for (int j = 0; j < 4; j++) {
			temp.m[i][j] /= div;
			inv.m[i][j] /= div;
		}

		// --- 掃き出し ---
		// 他の行のi列目をすべて0にする
		for (int k = 0; k < 4; k++) {
			if (k != i) {
				float mul = temp.m[k][i];
				for (int j = 0; j < 4; j++) {
					temp.m[k][j] -= temp.m[i][j] * mul;
					inv.m[k][j] -= inv.m[i][j] * mul;
				}
			}
		}
	}
	return inv;
}

// ===== 転置行列 =====
Matrix4x4 Transpose(const Matrix4x4& m) {
	Matrix4x4 result;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result.m[i][j] = m.m[j][i];
		}
	}
	return result;
}

// ===== 単位行列の作成 =====
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {0};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

// ===== 平行移動行列 =====
Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	return result;
}

// ===== 拡大縮小行列 =====
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = {0};
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	result.m[3][3] = 1.0f;
	return result;
}

// ===== 座標変換 =====
// 4x4行列を使ってVector3の座標変換を行う関数。ベクトルのw成分は1とする。
Vector3 TransformCoord(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result(0, 0, 0);
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	result.x /= w;
	result.y /= w;
	result.z /= w;
	return result;
}

// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 result = MakeIdentity4x4();
	result.m[1][1] = c;
	result.m[1][2] = s;
	result.m[2][1] = -s;
	result.m[2][2] = c;
	return result;
}

// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = c;
	result.m[0][2] = -s;
	result.m[2][0] = s;
	result.m[2][2] = c;
	return result;
}

// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float radian) {
	float s = std::sin(radian);
	float c = std::cos(radian);
	Matrix4x4 result = MakeIdentity4x4();
	result.m[0][0] = c;
	result.m[0][1] = s;
	result.m[1][0] = -s;
	result.m[1][1] = c;
	return result;
}

// 3次元アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
	Matrix4x4 result = Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);

	return result;
}

// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {0};
	float cot = 1.0f / std::tan(fovY / 2.0f);
	result.m[0][0] = cot / aspectRatio;
	result.m[1][1] = cot;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	return result;
}

// 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {0};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;
	return result;
}

// ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = {0};
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + (width / 2.0f);
	result.m[3][1] = top + (height / 2.0f);
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeUVTransformMatrix(const Transform& uvTransform) {
	Vector3 invScale = {
	    1.0f / uvTransform.scale.x,
	    1.0f / uvTransform.scale.y,
	    1.0f / uvTransform.scale.z,
	};
	Matrix4x4 mat = MakeScaleMatrix(invScale);
	//Matrix4x4 mat = MakeScaleMatrix(uvTransform.scale);
	mat = Multiply(mat, MakeRotateZMatrix(uvTransform.rotation.z));
	mat = Multiply(mat, MakeTranslateMatrix(uvTransform.translation));
	return mat;
}