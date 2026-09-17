#include "LightGizmo.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Texture/TextureManager.h"

// 静的メンバ変数
uint32_t LightGizmo::pointIconTextureHandle_ = 0;
uint32_t LightGizmo::spotIconTextureHandle_ = 0;
LightGizmoFlags LightGizmo::globalFlags_;
Camera* LightGizmo::camera_ = nullptr;
bool LightGizmo::isRecording_ = false;
Renderer::LineListConfig LightGizmo::rangeLines_;

namespace {
constexpr float kPI = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPI / 180.0f;    // 度 → ラジアン
constexpr uint32_t kCircleDivision = 32;     // 円を何本の線で描くか
constexpr uint32_t kSpotEdgeCount = 4;       // スポットライトの頂点から円へ引く線の本数
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
	pointIconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/pointLight.png");
	spotIconTextureHandle_ = TextureManager::Load("MyEngine/Resources/Textures/spotLight.png");
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
// アイコン（共通）
//=============================================================================
void LightGizmo::DrawIcon(const Vector3& position, uint32_t textureHandle) {
	Renderer::Rect3dConfig icon;
	icon.textureHandle = textureHandle;
	icon.shadingType = ShadingType::Unlit;
	icon.blendMode = BlendMode::Normal;
	icon.rasterizerType = RasterizerType::SolidNone;
	icon.depthMode = DepthMode::TestNoWrite; // 半透明として描く（深度を書かない。Skyboxの後に奥から順に描かれる）
	icon.isBillboard = true;
	icon.camera = camera_;
	icon.transform.translation = position;
	Renderer::DrawRect3d(icon);
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
		DrawIcon(light.position, pointIconTextureHandle_);
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


//=============================================================================
// スポットライト
//=============================================================================
void LightGizmo::AddSpotLight(const SpotLightComponent& light) {
	MY_ASSERT_MSG(isRecording_, "LightGizmo::Begin を呼んでから AddSpotLight を呼んでください");

	bool showIcon = globalFlags_.showIcon && light.gizmo.showIcon;
	bool showRange = globalFlags_.showRange && light.gizmo.showRange;

	// --- アイコン ---
	if (showIcon) {
		DrawIcon(light.position, spotIconTextureHandle_);
	}

	// --- 光の届く範囲（円錐） ---
	if (showRange) {
		// 向き（(0,0,0)なら真下。LightManagerがGPUへ送るときと同じ扱い）
		Vector3 direction = (LengthSq(light.direction) > 1e-6f) ? Normalize(light.direction) : Vector3{0.0f, -1.0f, 0.0f};

		// 向きと直交する2本の軸を作る（円を描く平面）
		// 外積は平行なベクトル同士だと0になるので、向きとほぼ平行なら別の軸を基準にする
		Vector3 reference = (std::abs(direction.y) < 0.99f) ? Vector3{0.0f, 1.0f, 0.0f} : Vector3{1.0f, 0.0f, 0.0f};
		Vector3 axisA = Normalize(Cross(reference, direction));
		Vector3 axisB = Cross(direction, axisA); // 直交する単位ベクトル同士の外積なので、長さは1

		// 底の円：頂点から斜めにrange進んだ所（光が届く距離の端）
		float outerAngle = std::clamp(light.outerAngle, 0.0f, SpotLightComponent::kMaxAngle) * kDegToRad;
		float radius = light.range * std::sin(outerAngle);                                  // 円の半径
		Vector3 center = light.position + direction * (light.range * std::cos(outerAngle)); // 円の中心
		PushCircle(rangeLines_.lines, center, radius, axisA, axisB, kRangeColor);

		// 頂点から円へ線を引いて、円錐に見せる
		for (uint32_t i = 0; i < kSpotEdgeCount; ++i) {
			float angle = 2.0f * kPI * static_cast<float>(i) / static_cast<float>(kSpotEdgeCount);
			Vector3 edge = center + axisA * (std::cos(angle) * radius) + axisB * (std::sin(angle) * radius);
			rangeLines_.lines.push_back({light.position, edge, kRangeColor});
		}
	}
}