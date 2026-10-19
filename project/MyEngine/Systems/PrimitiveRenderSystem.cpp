#include "PrimitiveRenderSystem.h"

#include <array>
#include <memory>

#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/ComponentSerializer.h"
#include "MyEngine/ECS/ComponentUI.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/ECS/GameComponent.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Math/Color.h"

namespace {
constexpr float kUnitHalfSize = 0.5f; // Scale が 1 のときの、球の半径・箱の1辺の半分（直径・1辺が1になるように）

/// <summary>
/// 描くのに使う共通の値（形によらない）
/// </summary>
template<class TConfig> 
void SetCommon(TConfig& config, const PrimitiveRendererComponent& component, Camera* camera) {
	config.color = ToColorCode(component.color);
	config.shadingType = component.isLit ? ShadingType::HalfLambert : ShadingType::Unlit;
	config.camera = camera;
}

// ===== 球：ワールド行列を 大きさ・回転・位置 に分けて、半径1の球を半分の大きさで描く =====
void DrawSphere(const TransformComponent& transform, const PrimitiveRendererComponent& component, Camera* camera) {
	Renderer::SphereConfig config;
	config.transform = DecomposeAffineMatrix(transform.worldMatrix);
	config.transform.scale = config.transform.scale * kUnitHalfSize;
	SetCommon(config, component, camera);
	Renderer::DrawSphere(config);
}

// ===== 箱：ワールド行列の1〜3行目が、それぞれ X・Y・Z 軸の向き × 大きさ。向きを軸に、長さの半分を大きさ（中心から面まで）にする =====
void DrawBox(const TransformComponent& transform, const PrimitiveRendererComponent& component, Camera* camera) {
	const Matrix4x4& m = transform.worldMatrix;
	Renderer::OBBConfig config;
	config.obb.center = GetWorldPosition(transform);
	std::array<float, 3> halfSizes = {}; // X・Y・Z の、中心から面までの長さ
	for (int axis = 0; axis < 3; ++axis) {
		const Vector3 row = {m.m[axis][0], m.m[axis][1], m.m[axis][2]};
		const float length = Length(row);
		if (length <= 0.0f) {
			return; // 大きさが0（向きが決まらない・見えない）
		}
		config.obb.axes[axis] = row / length;
		halfSizes[axis] = length * kUnitHalfSize;
	}
	config.obb.size = {halfSizes[0], halfSizes[1], halfSizes[2]};
	SetCommon(config, component, camera);
	Renderer::DrawOBB(config);
}

//=============================================================================
// Inspector と 保存
//=============================================================================
void DescribePrimitiveRenderer(ComponentUI& ui, PrimitiveRendererComponent& value) {
	ui.Field("Enabled###enabled", value.enabled, Tip("外すと描かない"));
	ui.Field("Shape###shape", value.shape, Tip("Sphere：球（Scale 1 で直径1）/ Box：箱（Scale 1 で1辺1）"));
	ui.ColorField("Color###color", value.color);
	ui.Field("Is Lit###isLit", value.isLit, Tip("ライトで陰を付ける（PBR）。外すと色そのまま"));
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void PrimitiveRenderSystem::Initialize() {
	// Primitive Renderer の置き場所・保存・Inspector を登録する（見せ方は DescribePrimitiveRenderer 1つ）
	EntityManager::RegisterComponent<PrimitiveRendererComponent>();
	ComponentSerializer::Register<PrimitiveRendererComponent>("Primitive Renderer", &DescribePrimitiveRenderer);
	ComponentEditorRegistry::Register(std::make_unique<DescribedComponentEditor<PrimitiveRendererComponent>>("Primitive Renderer", "Rendering3D", &DescribePrimitiveRenderer));
}


//=============================================================================
// 描画
//=============================================================================
void PrimitiveRenderSystem::Draw(Camera* camera) {
	EntityManager::ForEach<PrimitiveRendererComponent>([camera](Handle<Entity> entity, const PrimitiveRendererComponent& component) {
		if (!component.enabled || !EntityManager::IsActiveInHierarchy(entity)) {
			return;
		}
		const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
		if (transform == nullptr) {
			return;
		}
		if (component.shape == PrimitiveShape::Sphere) {
			DrawSphere(*transform, component, camera);
		} else {
			DrawBox(*transform, component, camera);
		}
	});
}