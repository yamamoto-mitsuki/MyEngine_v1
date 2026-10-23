#ifdef USE_IMGUI
#include "PolygonShapeGizmo.h"

#include <cmath>

#include "MyEngine/Components/PolygonShapeComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/Gizmos/PointListGizmo.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Math/Geometry/Polygon.h"
#include "MyEngine/Systems/PolygonShapeSystem.h"

namespace {
constexpr int kMinPoints = 3; // 点の数の下限（これより少なくは消せない）
} // namespace

//=============================================================================
// 毎フレーム
//=============================================================================
bool PolygonShapeGizmo::Draw(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered) {
	if (!PolygonShapeSystem::IsEditing()) {
		return false;
	}
	// --- 選んでいる物に Polygon Shape が無ければ、編集を終わる（選ぶのをやめた・ほかの物を選んだ）---
	const Handle<Entity> selected = HierarchyWindow::GetSelected();
	PolygonShapeComponent* shape = EntityManager::Get<PolygonShapeComponent>(selected);
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(selected);
	if (shape == nullptr || transform == nullptr) {
		PolygonShapeSystem::SetEditing(false);
		return false;
	}
	// --- 点を動かすのは、前の面の上（見えている面の上に印を出す）。閉じた多角形で、辺が交差していたら赤 ---
	PointList list;
	list.storage = shape->points;
	list.count = &shape->pointCount;
	list.minCount = kMinPoints;
	list.isClosed = true;
	list.planeZ = -std::abs(shape->depth) * 0.5f;
	list.isValid = Polygon2D::IsSimple(shape->GetPoints());
	list.editor = ComponentEditorRegistry::Find<PolygonShapeComponent>();
	list.component = shape;
	if (!PointListGizmo::Draw(selected, list, transform->worldMatrix, camera, imageMin, imageSize, isHovered)) {
		PolygonShapeSystem::SetEditing(false); // Esc で終わった
		return false;
	}
	return true;
}
#endif