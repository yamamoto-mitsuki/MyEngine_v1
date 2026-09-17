#include "LightManager.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#ifdef USE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Light/LightGizmo.h"

// 静的メンバ変数
LightManager* LightManager::instance_ = nullptr;

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン

/// <summary>
/// sRGB（見た目の色） → リニア（ライティング計算用）。1成分分
/// <para>GPUが _SRGB形式のテクスチャを読むときと同じ式</para>
/// </summary>
float SrgbToLinear(float c) {
	if (c <= 0.04045f) {
		return c / 12.92f;
	}
	return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/// <summary>
/// sRGBの色(RGB) → リニアの色(RGBA)。アルファはシェーダーで使わないので1固定
/// </summary>
Vector4 SrgbToLinear(const Vector3& color) { return {SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), 1.0f}; }

/// <summary>
/// 向きを正規化する。(0,0,0)だとシェーダーで壊れるので、そのときは真下にする
/// </summary>
Vector3 NormalizeDirection(const Vector3& direction) {
	if (LengthSq(direction) > 1e-6f) {
		return Normalize(direction);
	}
	return {0.0f, -1.0f, 0.0f};
}
} // namespace


//=============================================================================
// 初期化 / 解放
//=============================================================================
void LightManager::Initialize() {
	MY_ASSERT_MSG(instance_ == nullptr, "Initialize()が2回以上呼ばれています");
	instance_ = new LightManager();
}

void LightManager::Release() {
	delete instance_;
	instance_ = nullptr;
}


//=============================================================================
// 更新
//=============================================================================
void LightManager::Update() {
	instance_->CollectForGPU();
	instance_->FlushRemovals();
}

// ====== Component → GPU用データにまとめてRendererへ渡す =====
void LightManager::CollectForGPU() {
	// --- 平行光源 ---
	DirectionalLightData directional;
	directional.color = SrgbToLinear(directionalLight_.color);
	directional.intensity = directionalLight_.intensity;
	directional.direction = NormalizeDirection(directionalLight_.direction);

	// --- ポイントライト（先頭から上限まで） ---
	PointLightListData pointList;
	uint32_t pointCount = 0;
	for (const PointLightComponent& light : pointLights_) {
		if (pointCount >= kMaxPointLights) {
			if (!hasWarnedPointLightLimit_) {
				LogManager::Warning(std::format("ポイントライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxPointLights));
				hasWarnedPointLightLimit_ = true;
			}
			break;
		}
		PointLightData& data = pointList.lights[pointCount];
		data.color = SrgbToLinear(light.color);
		data.position = light.position;
		data.intensity = light.intensity;
		data.radius = light.radius; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		++pointCount;
	}
	pointList.count = pointCount;

	// --- スポットライト（先頭から上限まで） ---
	SpotLightListData spotList;
	uint32_t spotCount = 0;
	for (const SpotLightComponent& light : spotLights_) {
		if (spotCount >= kMaxSpotLights) {
			if (!hasWarnedSpotLightLimit_) {
				LogManager::Warning(std::format("スポットライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxSpotLights));
				hasWarnedSpotLightLimit_ = true;
			}
			break;
		}
		SpotLightData& data = spotList.lights[spotCount];
		data.color = SrgbToLinear(light.color);
		data.position = light.position;
		data.intensity = light.intensity;
		data.direction = NormalizeDirection(light.direction);
		data.range = light.range; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		// 角度（度）→ cos。内側が外側より大きいと明るさの向きが逆になるので、外側までに収める
		float outerAngle = std::clamp(light.outerAngle, 0.0f, SpotLightComponent::kMaxAngle);
		float innerAngle = std::clamp(light.innerAngle, 0.0f, outerAngle);
		data.cosOuter = std::cos(outerAngle * kDegToRad);
		data.cosInner = std::cos(innerAngle * kDegToRad);
		++spotCount;
	}
	spotList.count = spotCount;

	Renderer::SetFrameLights(directional, pointList, spotList);
}

// ====== 削除予約を反映する =====
void LightManager::FlushRemovals() {
	for (Handle<PointLightComponent> handle : pendingRemovePointLights_) {
		pointLights_.Destroy(handle);
	}
	pendingRemovePointLights_.clear();

	for (Handle<SpotLightComponent> handle : pendingRemoveSpotLights_) {
		spotLights_.Destroy(handle);
	}
	pendingRemoveSpotLights_.clear();
}


//=============================================================================
// ギズモ
//=============================================================================
void LightManager::DrawGizmos(Camera* camera) {
	LightGizmo::Begin(camera);
	for (const PointLightComponent& light : instance_->pointLights_) {
		LightGizmo::AddPointLight(light);
	}
	for (const SpotLightComponent& light : instance_->spotLights_) {
		LightGizmo::AddSpotLight(light);
	}
	LightGizmo::End();
}


//=============================================================================
// ポイントライト
//=============================================================================
Handle<PointLightComponent> LightManager::AddPointLight() { return instance_->pointLights_.Create(); }

void LightManager::RemovePointLight(Handle<PointLightComponent> handle) { instance_->pendingRemovePointLights_.push_back(handle); }

PointLightComponent* LightManager::GetPointLight(Handle<PointLightComponent> handle) { return instance_->pointLights_.Get(handle); }


//=============================================================================
// スポットライト
//=============================================================================
Handle<SpotLightComponent> LightManager::AddSpotLight() { return instance_->spotLights_.Create(); }

void LightManager::RemoveSpotLight(Handle<SpotLightComponent> handle) { instance_->pendingRemoveSpotLights_.push_back(handle); }

SpotLightComponent* LightManager::GetSpotLight(Handle<SpotLightComponent> handle) { return instance_->spotLights_.Get(handle); }


//=============================================================================
// 確認用ウィンドウ（Inspectorができるまでの仮）
//=============================================================================
#ifdef USE_IMGUI
void LightManager::DrawDebugWindow() {
	ImGui::Begin("Lights");

	// --- ギズモの表示（全体） ---
	LightGizmoFlags& globalFlags = LightGizmo::GetGlobalFlags();
	ImGui::Checkbox("Icon (All)", &globalFlags.showIcon);
	ImGui::SameLine();
	ImGui::Checkbox("Range (All)", &globalFlags.showRange);

	// --- 平行光源 ---
	if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
		DirectionalLightComponent& sun = instance_->directionalLight_;
		ImGui::PushID("Directional");
		ImGui::ColorEdit3("Color", &sun.color.x);
		ImGui::DragFloat3("Direction", &sun.direction.x, 0.01f);
		ImGui::DragFloat("Intensity", &sun.intensity, 0.01f, 0.0f, 100.0f);
		ImGui::PopID();
	}

	// --- ポイントライト ---
	if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("PointLights"); // スポットライトの欄と同じラベル（"Remove Added"など）があるので、IDの区切りを作る
		ImGui::Text("Count: %zu (GPU max %u)", instance_->pointLights_.Size(), kMaxPointLights);

		// 10個ずつ円状に並べて追加する。押すたびに1周り外側に置く
		if (ImGui::Button("Add x10")) {
			constexpr size_t kAddCount = 10;
			const Vector3 kColors[] = {
			    {1.0f, 0.3f, 0.3f},
			    {0.3f, 1.0f, 0.3f},
			    {0.3f, 0.3f, 1.0f},
			};
			float ringRadius = 4.0f + 3.0f * static_cast<float>(instance_->debugPointLights_.size() / kAddCount);
			for (size_t i = 0; i < kAddCount; ++i) {
				float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kAddCount);
				Handle<PointLightComponent> handle = AddPointLight();
				if (PointLightComponent* light = GetPointLight(handle)) {
					light->position = {std::cos(angle) * ringRadius, 1.0f, std::sin(angle) * ringRadius};
					light->color = kColors[i % 3];
					light->radius = 3.0f;
				}
				instance_->debugPointLights_.push_back(handle);
			}
		}
		ImGui::SameLine();
		// このウィンドウで追加したライトだけ消す（ゲーム側が追加したライトには触らない）
		if (ImGui::Button("Remove Added")) {
			for (Handle<PointLightComponent> handle : instance_->debugPointLights_) {
				RemovePointLight(handle);
			}
			instance_->debugPointLights_.clear();
		}

		// 一覧と編集（削除すると並び順が入れ替わるので、番号は目安）
		int index = 0;
		for (PointLightComponent& light : instance_->pointLights_) {
			ImGui::PushID(index);
			if (ImGui::TreeNode("PointLight", "Point %d", index)) {
				ImGui::DragFloat3("Position", &light.position.x, 0.05f);
				ImGui::ColorEdit3("Color", &light.color.x);
				ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 100.0f);
				ImGui::DragFloat("Radius", &light.radius, 0.05f, 0.0f, 1000.0f);
				ImGui::DragFloat("Decay", &light.decay, 0.01f, 0.0f, 10.0f);
				ImGui::Checkbox("Icon", &light.gizmo.showIcon);
				ImGui::SameLine();
				ImGui::Checkbox("Range", &light.gizmo.showRange);
				ImGui::TreePop();
			}
			ImGui::PopID();
			++index;
		}
		ImGui::PopID();
	}

	// --- スポットライト ---
	if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushID("SpotLights");
		ImGui::Text("Count: %zu (GPU max %u)", instance_->spotLights_.Size(), kMaxSpotLights);

		// 4個ずつ、高い所から真下を照らすように円状に並べる。押すたびに1周り外側に置く
		if (ImGui::Button("Add x4")) {
			constexpr size_t kAddCount = 4;
			const Vector3 kColors[] = {
			    {1.0f, 0.9f, 0.4f},
			    {0.4f, 0.9f, 0.8f},
			    {0.8f, 0.6f, 1.0f},
			};
			float ringRadius = 2.0f + 3.0f * static_cast<float>(instance_->debugSpotLights_.size() / kAddCount);
			for (size_t i = 0; i < kAddCount; ++i) {
				float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kAddCount);
				Handle<SpotLightComponent> handle = AddSpotLight();
				if (SpotLightComponent* light = GetSpotLight(handle)) {
					light->position = {std::cos(angle) * ringRadius, 4.0f, std::sin(angle) * ringRadius};
					light->direction = {0.0f, -1.0f, 0.0f};
					light->color = kColors[i % 3];
					light->range = 8.0f;
				}
				instance_->debugSpotLights_.push_back(handle);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove Added")) {
			for (Handle<SpotLightComponent> handle : instance_->debugSpotLights_) {
				RemoveSpotLight(handle);
			}
			instance_->debugSpotLights_.clear();
		}

		int index = 0;
		for (SpotLightComponent& light : instance_->spotLights_) {
			ImGui::PushID(index);
			if (ImGui::TreeNode("SpotLight", "Spot %d", index)) {
				ImGui::DragFloat3("Position", &light.position.x, 0.05f);
				ImGui::DragFloat3("Direction", &light.direction.x, 0.01f);
				ImGui::ColorEdit3("Color", &light.color.x);
				ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 100.0f);
				ImGui::DragFloat("Range", &light.range, 0.05f, 0.0f, 1000.0f);
				ImGui::DragFloat("Decay", &light.decay, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("Outer Angle", &light.outerAngle, 0.1f, 0.0f, SpotLightComponent::kMaxAngle);
				ImGui::DragFloat("Inner Angle", &light.innerAngle, 0.1f, 0.0f, light.outerAngle); // 外側より大きくできないようにする
				ImGui::Checkbox("Icon", &light.gizmo.showIcon);
				ImGui::SameLine();
				ImGui::Checkbox("Range Wire", &light.gizmo.showRange);
				ImGui::TreePop();
			}
			ImGui::PopID();
			++index;
		}
		ImGui::PopID();
	}

	ImGui::End();
}
#endif