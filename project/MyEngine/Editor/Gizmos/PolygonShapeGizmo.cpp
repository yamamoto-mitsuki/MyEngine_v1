#ifdef USE_IMGUI
#include "PolygonShapeGizmo.h"

#include <array>
#include <cmath>
#include <vector>
#include <cstddef>
#include <algorithm>

#include <externals/imgui/ImGuizmo.h>

#include "MyEngine/Camera/Camera.h"
#include "MyEngine/Collision/Collision.h"
#include "MyEngine/Components/PolygonShapeComponent.h"
#include "MyEngine/Components/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Math/Geometry/Polygon.h"
#include "MyEngine/Systems/PolygonShapeSystem.h"

namespace {
constexpr float kGridSize = 0.25f;                          // 点が吸い付く間隔（m。Entity から見た位置で）
constexpr int kMinPoints = 3;                               // 点の数の下限（これより少なくは消せない）
constexpr float kHandleHalfSize = 5.0f;                     // 点の印（■）の大きさの半分（ピクセル）
constexpr float kPlusHalfSize = 4.0f;                       // 辺の真ん中の印（＋）の大きさの半分（ピクセル）
constexpr float kPickRadius = 9.0f;                         // マウスがこれより近ければ、点・＋を掴める（ピクセル）
constexpr float kLineThickness = 2.0f;                      // 辺の線の太さ（ピクセル）
constexpr ImU32 kEdgeColor = IM_COL32(80, 220, 255, 255);   // 辺（水色）
constexpr ImU32 kInvalidColor = IM_COL32(255, 70, 70, 255); // 形がおかしい（辺が交差している）ときの辺（赤）
constexpr ImU32 kPointColor = IM_COL32(255, 255, 255, 255); // 点（白）
constexpr ImU32 kActiveColor = IM_COL32(255, 160, 40, 255); // 選んだ点・マウスが乗っている点と ＋（選んだ物の枠と同じオレンジ）
constexpr ImU32 kPlusColor = IM_COL32(80, 220, 255, 200);   // 辺の真ん中の ＋（水色）

/// <summary>
/// 編集の状態（エディタに1つだけ）
/// </summary>
struct EditState {
	Handle<Entity> entity;         // 編集している Entity（ほかの物を選んだら、状態を捨てる）
	int selectedPoint = -1;        // 選んでいる点（Delete で消す。-1 なら無し）
	int dragPoint = -1;            // ドラッグしている点（-1 ならしていない）
	std::vector<std::byte> before; // ドラッグ・足す・消すの前の Polygon Shape（ComponentEditor::Save の写し。Undo の「前」）
};

/// <summary>
/// 点の、Scene ビューの絵の上の位置
/// </summary>
struct ScreenPoints {
	std::array<ImVec2, kMaxPolygonPoints> pixels{};  // 絵の上のピクセル
	std::array<bool, kMaxPolygonPoints> isVisible{}; // 写っているか（カメラの後ろの点は false）
	int count = 0;                                   // 点の数
};

EditState edit;

// ===== 画面の位置（真ん中0、右と上が＋、端が±1）↔ Scene ビューの絵の上のピクセル（Y は下向き）=====
ImVec2 ToPixel(const Vector2& ndc, const ImVec2& imageMin, const ImVec2& imageSize) {
	return ImVec2(imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x, imageMin.y + (0.5f - ndc.y * 0.5f) * imageSize.y);
}
Vector2 ToNdc(const ImVec2& pixel, const ImVec2& imageMin, const ImVec2& imageSize) { return {(pixel.x - imageMin.x) / imageSize.x * 2.0f - 1.0f, 1.0f - (pixel.y - imageMin.y) / imageSize.y * 2.0f}; }

// 2点の距離の2乗（ピクセル）
float DistanceSq(const ImVec2& a, const ImVec2& b) {
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	return dx * dx + dy * dy;
}

// 2点の真ん中（ピクセル）
ImVec2 Midpoint(const ImVec2& a, const ImVec2& b) { return ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f); }

//=============================================================================
// 履歴（Undo）
//=============================================================================
// ===== 変える前の中身を覚える =====
void Remember(const PolygonShapeComponent& shape) {
	if (const ComponentEditor* editor = ComponentEditorRegistry::Find<PolygonShapeComponent>()) {
		edit.before = editor->Save(&shape);
	}
}

// ===== 覚えた中身と今の中身を、1件の履歴にする（Inspector で値を変えたときと同じ入口。変わっていなければ積まれない）=====
void Commit(const PolygonShapeComponent& shape) {
	const ComponentEditor* editor = ComponentEditorRegistry::Find<PolygonShapeComponent>();
	if (editor != nullptr && !edit.before.empty()) {
		EditorHistory::RecordComponentChange(edit.entity, *editor, edit.before, editor->Save(&shape));
		EditorHistory::CommitComponentChanges();
	}
	edit.before.clear();
}

//=============================================================================
// 位置の変換
//=============================================================================
// ===== 点を、前の面の上でワールドへ（見えている面の上に印を出す）=====
Vector3 ToWorld(const Vector2& point, const PolygonShapeComponent& shape, const Matrix4x4& world) { return TransformPoint({point.x, point.y, -std::abs(shape.depth) * 0.5f}, world); }

// ===== 点の、絵の上の位置 =====
ScreenPoints CalcScreenPoints(const PolygonShapeComponent& shape, const Matrix4x4& world, const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize) {
	ScreenPoints result;
	const std::span<const Vector2> points = shape.GetPoints();
	result.count = static_cast<int>(points.size());
	for (size_t i = 0; i < points.size(); ++i) {
		Vector2 ndc;
		result.isVisible[i] = camera.WorldToScreenPoint(ToWorld(points[i], shape, world), &ndc);
		if (result.isVisible[i]) {
			result.pixels[i] = ToPixel(ndc, imageMin, imageSize);
		}
	}
	return result;
}

// ===== マウスの位置 → Entity から見た X・Y（前の面の平面の上）。平面と交わらなければ false =====
bool MouseToLocal(const PolygonShapeComponent& shape, const Matrix4x4& world, const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, Vector2* local) {
	// マウスの所から奥へ向かうレイ（Scene ビューのクリックで選ぶときと同じ）
	const Ray ray = camera.ScreenPointToRay(ToNdc(ImGui::GetMousePos(), imageMin, imageSize));
	// 前の面の平面（向きは Entity の Z 軸。Plane は「法線 · 点 ＝ distance」で表す）
	const Vector3 normal = Normalize(TransformNormal({0.0f, 0.0f, 1.0f}, world));
	if (LengthSq(normal) == 0.0f) {
		return false; // Z の大きさが 0（平面の向きが決まらない）
	}
	const Vector3 onPlane = ToWorld({0.0f, 0.0f}, shape, world);
	float t = 0.0f;
	if (!Collision::RayPlane(ray, {normal, Dot(normal, onPlane)}, &t)) {
		return false; // 平面と平行・平面がカメラの後ろ
	}
	// 交わった所を、Entity から見た位置に戻す
	const Vector3 hit = TransformPoint(Collision::RayPoint(ray, t), Inverse(world));
	if (!std::isfinite(hit.x) || !std::isfinite(hit.y)) {
		return false; // 大きさが 0 の物（逆行列が作れない）
	}
	*local = {hit.x, hit.y};
	return true;
}

// ===== グリッドに吸い付ける（Ctrl を押している間はしない）=====
Vector2 Snap(const Vector2& point) {
	if (ImGui::GetIO().KeyCtrl) {
		return point;
	}
	return {std::round(point.x / kGridSize) * kGridSize, std::round(point.y / kGridSize) * kGridSize};
}

//=============================================================================
// 操作
//=============================================================================
// ===== マウスの近くの点（無ければ -1）=====
int FindHoveredPoint(const ScreenPoints& screen, const ImVec2& mouse) {
	int found = -1;
	float best = kPickRadius * kPickRadius;
	for (int i = 0; i < screen.count; ++i) {
		const float distanceSq = DistanceSq(mouse, screen.pixels[i]);
		if (screen.isVisible[i] && distanceSq < best) {
			found = i;
			best = distanceSq;
		}
	}
	return found;
}

// ===== マウスの近くの ＋（辺 i の真ん中。無ければ -1）=====
int FindHoveredPlus(const ScreenPoints& screen, const ImVec2& mouse) {
	int found = -1;
	float best = kPickRadius * kPickRadius;
	for (int i = 0; i < screen.count; ++i) {
		const int next = (i + 1) % screen.count;
		if (!screen.isVisible[i] || !screen.isVisible[next]) {
			continue;
		}
		const float distanceSq = DistanceSq(mouse, Midpoint(screen.pixels[i], screen.pixels[next]));
		if (distanceSq < best) {
			found = i;
			best = distanceSq;
		}
	}
	return found;
}

// ===== 辺 edge の真ん中に点を足す。足した点の番号を返す =====
int InsertPoint(PolygonShapeComponent& shape, int edge) {
	const int count = shape.pointCount;
	const int insert = edge + 1;
	const Vector2 middle = (shape.points[edge] + shape.points[(edge + 1) % count]) * 0.5f;
	// insert 番から後ろを1つずつ後ろへずらして、空いた所に入れる（copy_backward：後ろから写すので、重なっていても壊れない）
	std::copy_backward(shape.points.begin() + insert, shape.points.begin() + count, shape.points.begin() + count + 1);
	shape.points[insert] = middle;
	shape.pointCount = count + 1;
	return insert;
}

// ===== 点 index を消す（後ろの点を1つずつ前へ詰める）=====
void ErasePoint(PolygonShapeComponent& shape, int index) {
	std::copy(shape.points.begin() + index + 1, shape.points.begin() + shape.pointCount, shape.points.begin() + index);
	--shape.pointCount;
}

// ===== マウス：押す（点を掴む・＋で足す・何も無い所で選ぶのをやめる）・ドラッグ・離す =====
void HandleMouse(PolygonShapeComponent& shape, const Matrix4x4& world, const Camera& camera, const ScreenPoints& screen, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered) {
	// --- 押した（右上の軸の上は、そちらの操作）---
	const bool canGrab = isHovered && edit.dragPoint < 0 && !ImGuizmo::IsViewManipulateHovered();
	if (canGrab && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse = ImGui::GetMousePos();
		const int hoveredPoint = FindHoveredPoint(screen, mouse);
		const int hoveredPlus = (shape.pointCount < static_cast<int>(kMaxPolygonPoints)) ? FindHoveredPlus(screen, mouse) : -1;
		if (hoveredPoint >= 0) {
			Remember(shape);
			edit.selectedPoint = edit.dragPoint = hoveredPoint;
		} else if (hoveredPlus >= 0) {
			Remember(shape); // 足す前を覚える（足してドラッグして離すまでが1件）
			edit.selectedPoint = edit.dragPoint = InsertPoint(shape, hoveredPlus);
		} else {
			edit.selectedPoint = -1;
		}
	}
	if (edit.dragPoint < 0) {
		return;
	}
	// --- ドラッグ中：マウスの所へ（グリッドに吸い付く）。離したら1件の履歴にする ---
	// 少し動かすまで（IsMouseDragging：押した所から数ピクセル）は点を動かさない。押しただけで、グリッドに吸い付いて位置が変わらないように
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		Vector2 local;
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && MouseToLocal(shape, world, camera, imageMin, imageSize, &local)) {
			shape.points[edit.dragPoint] = Snap(local);
		}
	} else {
		Commit(shape);
		edit.dragPoint = -1;
	}
}

// ===== キー：Delete で選んだ点を消す（3 点より少なくはしない）=====
void HandleDelete(PolygonShapeComponent& shape) {
	if (edit.dragPoint >= 0 || edit.selectedPoint < 0 || edit.selectedPoint >= shape.pointCount || !ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		return;
	}
	if (shape.pointCount <= kMinPoints) {
		return;
	}
	Remember(shape);
	ErasePoint(shape, edit.selectedPoint);
	Commit(shape);
	edit.selectedPoint = -1;
}

//=============================================================================
// 描く
//=============================================================================
void DrawShape(const PolygonShapeComponent& shape, const ScreenPoints& screen, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true); // 絵の外（黒い余白）には描かない
	const ImVec2 mouse = ImGui::GetMousePos();
	const int count = screen.count;
	// --- 辺（形がおかしいと赤）---
	const ImU32 edgeColor = Polygon2D::IsSimple(shape.GetPoints()) ? kEdgeColor : kInvalidColor;
	for (int i = 0; i < count; ++i) {
		const int next = (i + 1) % count;
		if (screen.isVisible[i] && screen.isVisible[next]) {
			drawList->AddLine(screen.pixels[i], screen.pixels[next], edgeColor, kLineThickness);
		}
	}
	// --- 辺の真ん中の ＋（点を足せる間だけ。ドラッグ中は出さない）---
	if (count < static_cast<int>(kMaxPolygonPoints) && edit.dragPoint < 0) {
		const int hoveredPlus = isHovered ? FindHoveredPlus(screen, mouse) : -1;
		for (int i = 0; i < count; ++i) {
			const int next = (i + 1) % count;
			if (!screen.isVisible[i] || !screen.isVisible[next]) {
				continue;
			}
			const ImVec2 center = Midpoint(screen.pixels[i], screen.pixels[next]);
			const ImU32 color = (i == hoveredPlus) ? kActiveColor : kPlusColor;
			drawList->AddLine(ImVec2(center.x - kPlusHalfSize, center.y), ImVec2(center.x + kPlusHalfSize, center.y), color, kLineThickness);
			drawList->AddLine(ImVec2(center.x, center.y - kPlusHalfSize), ImVec2(center.x, center.y + kPlusHalfSize), color, kLineThickness);
		}
	}
	// --- 点（■。選んだ点・マウスが乗っている点はオレンジ）---
	const int hoveredPoint = (isHovered && edit.dragPoint < 0) ? FindHoveredPoint(screen, mouse) : -1;
	for (int i = 0; i < count; ++i) {
		if (!screen.isVisible[i]) {
			continue;
		}
		const ImVec2& p = screen.pixels[i];
		const bool isActive = (i == edit.selectedPoint || i == hoveredPoint);
		drawList->AddRectFilled(ImVec2(p.x - kHandleHalfSize, p.y - kHandleHalfSize), ImVec2(p.x + kHandleHalfSize, p.y + kHandleHalfSize), isActive ? kActiveColor : kPointColor);
	}
	drawList->PopClipRect();
}
} // namespace

//=============================================================================
// 毎フレーム
//=============================================================================
bool PolygonShapeGizmo::Draw(const Camera& camera, const ImVec2& imageMin, const ImVec2& imageSize, bool isHovered) {
	if (!PolygonShapeSystem::IsEditing()) {
		edit = {};
		return false;
	}
	// --- 選んでいる物に Polygon Shape が無ければ、編集を終わる（選ぶのをやめた・ほかの物を選んだ）---
	const Handle<Entity> selected = HierarchyWindow::GetSelected();
	PolygonShapeComponent* shape = EntityManager::Get<PolygonShapeComponent>(selected);
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(selected);
	if (shape == nullptr || transform == nullptr) {
		PolygonShapeSystem::SetEditing(false);
		edit = {};
		return false;
	}
	if (edit.entity != selected) {
		edit = {};
		edit.entity = selected; // 別の Polygon Shape を選んだ：選んだ点などは捨てる
	}
	// --- キー（Scene ビューを触っている時だけ。文字の入力中は何もしない）---
	const bool isKeyTarget = (isHovered || ImGui::IsWindowFocused()) && !ImGui::GetIO().WantTextInput;
	if (isKeyTarget && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		if (edit.dragPoint >= 0) {
			Commit(*shape); // ドラッグ中なら、そこまでを1件にする
		}
		PolygonShapeSystem::SetEditing(false);
		edit = {};
		return false;
	}
	if (isKeyTarget) {
		HandleDelete(*shape);
	}
	// --- マウス（変えた後の形で描くように、描くための位置は後でもう一度出す）---
	const Matrix4x4& world = transform->worldMatrix;
	HandleMouse(*shape, world, camera, CalcScreenPoints(*shape, world, camera, imageMin, imageSize), imageMin, imageSize, isHovered);
	DrawShape(*shape, CalcScreenPoints(*shape, world, camera, imageMin, imageSize), imageMin, imageSize, isHovered);
	return true;
}
#endif