#ifdef USE_IMGUI
#include "ArrowStripGizmo.h"

#include "MyEngine/Components/ArrowStripComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/Gizmos/PointListGizmo.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Math/Geometry/PathCurve.h"
#include "MyEngine/Systems/ArrowStripSystem.h"

namespace {
constexpr int kMinPoints = 2; // 点の数の下限（まっすぐな線）
PathCurve preview;            // 描く線（Entity から見た座標。毎フレーム作り直して、配列は使い回す）
} // namespace

//=============================================================================
// 毎フレーム
//=============================================================================
bool ArrowStripGizmo::Draw(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered) {
	if (!ArrowStripSystem::IsEditing()) {
		return false;
	}
	// --- 選んでいる物に Arrow Strip が無ければ、編集を終わる（選ぶのをやめた・ほかの物を選んだ）---
	const Handle<Entity> selected = HierarchyWindow::GetSelected();
	ArrowStripComponent* strip = EntityManager::Get<ArrowStripComponent>(selected);
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(selected);
	if (strip == nullptr || transform == nullptr) {
		ArrowStripSystem::SetEditing(false);
		return false;
	}
	// --- 点は矢印を描く面（Z が 0）の上で動かす。開いた線で、点を通る曲線も描く ---
	ArrowStripSystem::BuildPath(*strip, MakeIdentity4x4(), preview);
	PointList list;
	list.storage = strip->points;
	list.count = &strip->pointCount;
	list.minCount = kMinPoints;
	list.isClosed = false;
	list.planeZ = 0.0f;
	list.curve = preview.GetPolyline();
	list.editor = ComponentEditorRegistry::Find<ArrowStripComponent>();
	list.component = strip;
	if (!PointListGizmo::Draw(selected, list, transform->worldMatrix, camera, imageMin, imageSize, isHovered)) {
		ArrowStripSystem::SetEditing(false); // Esc で終わった
		return false;
	}
	return true;
}
#endif