#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Math/Vector4.h"

// 色の書き方の変換
// Component・Inspector は 0〜1 の Vector4（RGBA。色を選びやすい）、Renderer は 0xRRGGBBAA（1色8ビットずつ）で持つ
// 色の空間：Inspector・コードで書く色は sRGB（色見本・画像と同じ、見た目の値）。シェーダーへ送るときにリニア（光の強さに比例する値）へ直す
//   シェーダーはリニアで計算して、最後に画面（_SRGB の形）へ書くときに GPU が sRGB へ戻す。画像も _SRGB の形で読むので、読むときにリニアになる
//   色だけ直さないと、色見本で選んだ色より明るく・薄く出る（0.2 の暗い灰色が、画面では 0.48 の灰色になる）

// 0〜1 の Vector4 → 0xRRGGBBAA（0〜1 の外の値は、0 か 1 にそろえる。隣の色にはみ出さないように）
inline uint32_t ToColorCode(const Vector4& color) {
	const auto toByte = [](float value) { return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
	return (toByte(color.x) << 24) | (toByte(color.y) << 16) | (toByte(color.z) << 8) | toByte(color.w);
}

// 0xRRGGBBAA → 0〜1 の Vector4（8ビットなので、ToColorCode で戻しても値は変わらない）
inline Vector4 FromColorCode(uint32_t code) {
	return {
	    static_cast<float>((code >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((code >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((code >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(code & 0xFF) / 255.0f,
	};
}

// A（透け方）にだけ倍率を掛ける（Alpha Group でまとめて透明にする。RGB はそのまま）
inline Vector4 MultiplyAlpha(const Vector4& color, float alpha) { return {color.x, color.y, color.z, color.w * alpha}; }

// 色と色を掛ける（RGBA をそれぞれ掛ける。白 (1,1,1,1) を掛けても変わらない。パーティクルの「初めの色 × 最後の色」）
inline Vector4 MultiplyColor(const Vector4& a, const Vector4& b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

// ===== sRGB（見た目の値）→ リニア（シェーダーで計算する値）=====
// GPU が _SRGB の形の画像を読むときと同じ式（暗い所は直線、それより上は 2.4 乗の曲線）。0 と 1 は変わらない

// 1色分
inline float SrgbToLinear(float value) {
	if (value <= 0.04045f) {
		return value / 12.92f;
	}
	return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

// RGB（光の色・自己発光・霧の色など、A の無い色）
inline Vector3 SrgbToLinear(const Vector3& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z)}; }

// RGBA（A は透け方の割合なので、直さない）
inline Vector4 SrgbToLinear(const Vector4& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), color.w}; }

// 0xRRGGBBAA（sRGB）→ シェーダーへ送るリニアの Vector4（Renderer が、色の番号を Material に入れるときに使う）
inline Vector4 FromColorCodeLinear(uint32_t code) { return SrgbToLinear(FromColorCode(code)); }