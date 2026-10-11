#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "MyEngine/Math/Vector4.h"

// 色の書き方の変換
// Component・Inspector は 0〜1 の Vector4（RGBA。色を選びやすい）、Renderer は 0xRRGGBBAA（1色8ビットずつ）で持つ

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