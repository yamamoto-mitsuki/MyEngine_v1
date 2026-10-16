#include "GizmoLines.h"

#include <cmath>
#include <numbers>

namespace {
constexpr float kPI = std::numbers::pi_v<float>;
} // namespace


//=============================================================================
// 円
//=============================================================================
void GizmoLines::PushCircle(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius, 
	const Vector3& axisA, const Vector3& axisB, uint32_t color, uint32_t division) {
	Vector3 prev = center + axisA * radius; // 角度0の点
	for (uint32_t i = 1; i <= division; ++i) {
		const float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(division);
		const Vector3 current = center + axisA * (std::cos(angle) * radius) + axisB * (std::sin(angle) * radius);
		lines.push_back({prev, current, color});
		prev = current; // 次の線の始点にする
	}
}


//=============================================================================
// 球
//=============================================================================
void GizmoLines::PushWireSphere(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius,
	uint32_t color, uint32_t division) { 
	const Vector3 axisX = {1.0f, 0.0f, 0.0f};
	const Vector3 axisY = {0.0f, 1.0f, 0.0f};
	const Vector3 axisZ = {0.0f, 0.0f, 1.0f};
	PushCircle(lines, center, radius, axisX, axisY, color, division); // XY平面
	PushCircle(lines, center, radius, axisY, axisZ, color, division); // YZ平面
	PushCircle(lines, center, radius, axisZ, axisX, color, division); // ZX平面
}