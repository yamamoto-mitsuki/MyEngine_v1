#pragma once
#include <type_traits>

#include "MyEngine/Math/Vector4.h"

// モデルのファイルを使わずに、決まった形（球・箱）を描く。Entityに付けるComponent（カテゴリ：Rendering3D）
// 大きさは Transform の Scale（Scale が 1 なら、球は直径1・箱は1辺1。Collider の初期値と同じ大きさ）
// 描くのは PrimitiveRenderSystem。Unity の「3D Object → Sphere / Cube」の見た目の所と同じ

/// <summary>
/// 形
/// </summary>
enum class PrimitiveShape {
	Sphere, // 球
	Box,    // 箱
};

/// <summary>
/// 決まった形を描く設定
/// </summary>
struct PrimitiveRendererComponent {
	bool enabled = true;                        // false の間は描かない
	PrimitiveShape shape = PrimitiveShape::Box; // 形
	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};   // 色（RGBA。0〜1）
	bool isLit = true;                          // true：ライトで陰が付く（PBR）／false：色そのまま（Unlit）
};

static_assert(std::is_trivially_copyable_v<PrimitiveRendererComponent>);