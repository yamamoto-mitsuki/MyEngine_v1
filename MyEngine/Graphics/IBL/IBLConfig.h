#pragma once
#include <cstdint>

// IBLで焼くマップの解像度・段数を一元に管理
namespace IBLConfig {
inline constexpr uint32_t kEnvironmentSize = 512; // Envキューブ1面
inline constexpr uint32_t kIrradianceSize = 32;   // 拡散は低解像度で十分
inline constexpr uint32_t kPrefilterSize = 128;   // 鏡面ベース解像度
inline constexpr uint32_t kPrefilterMipCount = 5; // ラフネス段階
inline constexpr uint32_t kBrdfLutSize = 512;     // 2D LUT
}