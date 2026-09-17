#include "LightGizmo.h"

#include <cmath>
#include <vector>
#include <numbers>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

// 静的メンバ変数
uint32_t LightGizmo::iconTextureHandle_ = 0;
LightGizmoFlags LightGizmo::globalFlags_;
Camera* LightGizmo::camera_ = nullptr;
bool LightGizmo::isRecording_ = false;
Renderer::LineListConfig LightGizmo::rangeLines_;

namespace {
constexpr float kPI = std::numbers::pi_v<float>;
constexpr uint32_t kCircleDivision = 32;     // 円を何本の線で描くか
constexpr uint32_t kRangeColor = 0xFFA500FF; // 範囲の線の色（オレンジ）
constexpr float kNoFadeStart = 10000.0f;     // 距離でフェードさせないための値
constexpr float kNoFadeEnd = 20000.0f;

/// <summary>
/// axisAとaxisBが作る平面上に、中心から半径radiusの円を線で並べる
/// <para>円周上の点 = 中心 + cos(角度) * axisA + sin(角度) * axisB</para>
/// </summary>
void PushCircle(std::vector<Renderer::LineSegment>& lines, const Vector3& center, float radius, const Vector3& axisA, const Vector3& axisB, uint32_t color) {
	Vector3 prev = center + axisA * radius; // 角度0の点
	for (uint32_t i = 1; i <= kCircleDivision; ++i) {
		float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kCircleDivision);
		Vector3 current = center + axisA * (std::cos(angle) * radius) + axisB * (std::sin(angle) * radius);
		lines.push_back({prev, current, color});
		prev = current; // 次の線の始点にする
	}
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void LightGizmo::Initialize() {
	iconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/PointLight.png");
	// フェードしない値にする
	rangeLines_.fadeStartDistance = kNoFadeStart;
	rangeLines_.fadeEndDistance = kNoFadeEnd;
}


//=============================================================================
// 開始 / 終了
//=============================================================================
void LightGizmo::Begin(Camera* camera) {
	MY_ASSERT_MSG(!isRecording_, "LightGizmo::End を呼ぶ前に Begin が呼ばれました");
	isRecording_ = true;
	camera_ = camera;
	rangeLines_.camera = camera;
	rangeLines_.lines.clear(); // 中身だけ消す。確保済みのメモリは残るので、次から再確保が起きにくい
}

void LightGizmo::End() {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼ばずに End が呼ばれました");
	Renderer::DrawLines(rangeLines_); // 線が0本なら中で何もしない
	camera_ = nullptr;                // フレームをまたいで持たない
	rangeLines_.camera = nullptr;
	isRecording_ = false;
}


//=============================================================================
// ポイントライト
//=============================================================================
void LightGizmo::AddPointLight(const PointLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddPointLight を呼んでください");

	// 全体とライトごと、両方ONのときだけ出す
	bool showIcon = globalFlags_.showIcon && light.gizmo.showIcon;
	bool showRange = globalFlags_.showRange && light.gizmo.showRange;

	// --- アイコン ---
	if (showIcon) {
		Renderer::Rect3dConfig icon;
		icon.textureHandle = iconTextureHandle_;
		icon.shadingType = ShadingType::Unlit;
		icon.blendMode = BlendMode::Normal;
		icon.rasterizerType = RasterizerType::SolidNone;
		icon.isBillboard = true;
		icon.camera = camera_;
		icon.transform.scale.y = 1.5f;
		icon.transform.translation = light.position;
		Renderer::DrawRect3d(icon);
	}

	// --- 光の届く範囲。3方向の円を重ねて球に見せる。描くのはEndでまとめて ---
	if (showRange) {
		const Vector3 axisX = {1.0f, 0.0f, 0.0f};
		const Vector3 axisY = {0.0f, 1.0f, 0.0f};
		const Vector3 axisZ = {0.0f, 0.0f, 1.0f};
		PushCircle(rangeLines_.lines, light.position, light.radius, axisX, axisY, kRangeColor); // XY平面
		PushCircle(rangeLines_.lines, light.position, light.radius, axisY, axisZ, kRangeColor); // YZ平面
		PushCircle(rangeLines_.lines, light.position, light.radius, axisZ, axisX, kRangeColor); // ZX平面
	}
}