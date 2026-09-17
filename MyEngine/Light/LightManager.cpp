#include "LightManager.h"

#include <cmath>
#include <format>

#include "MyEngine/Diagnostics/MyAssert.h"
#include "MyEngine/Diagnostics/LogManager.h"
#include "MyEngine/Light/LightGizmo.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"

// 静的メンバ変数
LightManager* LightManager::instance_ = nullptr;

namespace {
/// <summary>
/// sRGB（見た目の色） → リニア（ライティング計算用）。1成分分
/// <para>GPUが _SRGB形式の手クスユアを読むときと同じ式</para>
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
}


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
	// 向きが(0,0,0)だとシェーダーのnormalizeで壊れるので、真下にしておく
	if (LengthSq(directionalLight_.direction) > 1e-6f) {
		directional.direction = Normalize(directionalLight_.direction);
	} else {
		directional.direction = {0.0f, -1.0f, 0.0f};
	}

	// --- ポイントライト（先頭から上限まで） ---
	PointLightListData pointList;
	uint32_t count = 0;
	for (const PointLightComponent& light : pointLights_) {
		if (count >= kMaxPointLights) {
			if (!hasWarnedPointLightLimit_) {
				LogManager::Warning(std::format("ポイントライトが上限({}個)を超えています。超えた分は描画に使われません", kMaxPointLights));
				hasWarnedPointLightLimit_ = true;
			}
			break;
		}
		PointLightData& data = pointList.lights[count];
		data.color = SrgbToLinear(light.color);
		data.position = light.position;
		data.intensity = light.intensity;
		data.radius = light.radius; // 0や負の値はシェーダー側で安全な値に丸めている
		data.decay = light.decay;
		++count;
	}
	pointList.count = count;

	Renderer::SetFrameLights(directional, pointList);
}

// ====== 削除予約を反映する =====
void LightManager::FlushRemovals() {
	for (Handle<PointLightComponent> handle : pendingRemovePointLights_) {
		pointLights_.Destroy(handle);
	}
	pendingRemovePointLights_.clear();
}


//=============================================================================
// ギズモ
//=============================================================================
void LightManager::DrawGizmos(Camera* camera) {
	LightGizmo::Begin(camera);
	for (const PointLightComponent& light : instance_->pointLights_) {
		LightGizmo::AddPointLight(light);
	}
	LightGizmo::End();
}


//=============================================================================
// ポイントライト
//=============================================================================
Handle<PointLightComponent> LightManager::AddPointLight() { return instance_->pointLights_.Create(); }

void LightManager::RemovePointLight(Handle<PointLightComponent> handle) { instance_->pendingRemovePointLights_.push_back(handle); }

PointLightComponent* LightManager::GetPointLight(Handle<PointLightComponent> handle) { return instance_->pointLights_.Get(handle); }