#pragma once
#include <vector>
#include <cstdint>

#include "MyEngine/Math/Vector3.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

/// <summary>
/// ギズモの線の形を作る（renderer::LineListConfig::linesに足していく）
/// </summary>
namespace GizmoLines {
constexpr uint32_t kDefaultCircleDivision = 24; // 円を何本の線で描くか

/// <summary>
/// axisA と axisB が作る平面に、中心center・半径radiusの円を線で並べる
/// </summary>
/// <param name="division">何本の線で描くか</param>
void PushCircle(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius, const Vector3& axisA,
	const Vector3& axisB, uint32_t color, uint32_t division = kDefaultCircleDivision);

/// <summary>
/// 球: XY・YZ・ZX の3方向の円を重ねる
/// </summary>
void PushWireSphere(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius, uint32_t color,
	uint32_t division = kDefaultCircleDivision);
} // namespace GizmoLines