#ifdef USE_IMGUI
#include "UIGizmo.h"

#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

#include "MyEngine/Components/UI/ImageComponent.h"
#include "MyEngine/Components/UI/TextComponent.h"
#include "MyEngine/Components/Core/TransformComponent.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Viewport/EditorViewport.h"
#include "MyEngine/Editor/Viewport/SceneVisibility.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Graphics/Renderer/Renderer.h"
#include "MyEngine/Systems/Rendering2D/SpriteRenderSystem.h"
#include "MyEngine/Systems/UI/TextRenderSystem.h"

namespace {
constexpr ImU32 kSelectedColor = IM_COL32(255, 160, 40, 255); // 選んだ UI の枠（3D の物の枠と同じ色）
constexpr ImU32 kHoveredColor = IM_COL32(255, 160, 40, 110);  // マウスが乗っている UI の枠（うすく。クリックすると選べる物）
constexpr float kOutlineThickness = 1.5f;                     // 枠の線の太さ（画面のピクセル）
constexpr float kPivotRadius = 3.5f;                          // 位置（Pivot の所）の印の半径（画面のピクセル）

/// <summary>
/// 画面に重ねる UI の置いた形（正規化座標）
/// </summary>
struct UIShape {
	ScreenQuad quad;  // 四角の4つの角
	Vector2 position; // Entity の位置（Pivot の所）
};

/// <summary>
/// UI を押してから離すまでの状態（離したときに、押す前と後を1件の履歴にする）
/// </summary>
struct UIDragState {
	bool isPressed = false;        // UI の上で押して、まだ離していないか
	bool isDragging = false;       // 押したまま少し動かしたか（動かすまでは位置を変えない。クリックで選ぶだけのとき、ずれないように）
	Handle<Entity> entity;         // 動かしている物
	std::vector<std::byte> before; // 押したときの Transform（ComponentEditor::Save の写し）
	Vector3 startTranslation;      // 押したときの Translation
	Matrix4x4 parentInverse;       // 親のワールド行列の逆行列（画面で動かした量を、親から見た量に直す）
	ImVec2 startMouse;             // 押したときのマウスの位置（画面の座標）
};
UIDragState drag;

//=============================================================================
// UI の形
//=============================================================================
// 画面に重ねる UI の形（Image があれば Size の四角、無ければ Text の文字のかたまり）
// どちらも無い・描いていない（無効・目で隠した）・選べない（画面の編集中の、画面の外の物）物は false
bool CalcShape(Handle<Entity> entity, UIShape& shape) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	if (transform == nullptr || !EntityManager::IsActiveInHierarchy(entity) || !SceneVisibility::IsPickable(entity)) {
		return false;
	}
	const Matrix4x4& m = transform->worldMatrix;
	if (const ImageComponent* image = EntityManager::Get<ImageComponent>(entity); image != nullptr && image->enabled) {
		shape.quad = SpriteRenderSystem::CalcScreenQuad(m, image->anchor, image->pivot, image->size); // Fill は掛けない（Image の大きさ全部）
		shape.position = SpriteRenderSystem::CalcScreenPoint(m, image->anchor, {0.0f, 0.0f});
		return true;
	}
	if (const TextComponent* text = EntityManager::Get<TextComponent>(entity); text != nullptr && text->enabled) {
		shape.quad = SpriteRenderSystem::CalcScreenQuad(m, text->anchor, text->pivot, TextRenderSystem::CalcTextSize(entity));
		shape.position = SpriteRenderSystem::CalcScreenPoint(m, text->anchor, {0.0f, 0.0f});
		return true;
	}
	return false;
}

// 画面の座標（Scene ビューの絵の上。Y は下向き）→ 正規化座標（真ん中が0、右と上が＋、端が±1）
Vector2 ToNdc(const ImVec2& point, const ImVec2& imageMin, const ImVec2& imageSize) { return {(point.x - imageMin.x) / imageSize.x * 2.0f - 1.0f, 1.0f - (point.y - imageMin.y) / imageSize.y * 2.0f}; }

// 正規化座標 → 画面の座標（ToNdc の逆）
ImVec2 ToImage(const Vector2& ndc, const ImVec2& imageMin, const ImVec2& imageSize) { return {imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x, imageMin.y + (0.5f - ndc.y * 0.5f) * imageSize.y}; }

//=============================================================================
// クリックで選ぶ
//=============================================================================
// point の下にある UI のうち、一番手前に描いてある物（描く順と同じ：Order が大きい物。同じなら番号が大きい＝後に作った物）
Handle<Entity> PickUI(const Vector2& point) {
	Handle<Entity> best;
	int bestOrder = 0;
	EntityId bestId = 0;
	const auto consider = [&](Handle<Entity> entity, int order) {
		UIShape shape;
		if (!CalcShape(entity, shape) || !SpriteRenderSystem::IsInsideQuad(point, shape.quad)) {
			return;
		}
		const Entity* owner = EntityManager::Get(entity);
		const EntityId id = (owner != nullptr) ? owner->id : 0;
		// std::tie：「1つ目を比べて、同じなら2つ目」をまとめて書ける（描く順の並べ方と同じ）
		if (!best.IsValid() || std::tie(order, id) > std::tie(bestOrder, bestId)) {
			best = entity;
			bestOrder = order;
			bestId = id;
		}
	};
	EntityManager::ForEach<ImageComponent>([&consider](Handle<Entity> entity, const ImageComponent& image) { consider(entity, image.order); });
	EntityManager::ForEach<TextComponent>([&consider](Handle<Entity> entity, const TextComponent& text) { consider(entity, text.order); });
	return best;
}

//=============================================================================
// ドラッグで動かす
//=============================================================================
// 押した：今の Transform を覚える（動かし始めるのは、少し動かしてから）
void BeginPress(Handle<Entity> entity) {
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(entity);
	const ComponentEditor* editor = ComponentEditorRegistry::Find<TransformComponent>();
	if (transform == nullptr || editor == nullptr) {
		return;
	}
	drag.isPressed = true;
	drag.isDragging = false;
	drag.entity = entity;
	drag.before = editor->Save(transform);
	drag.startTranslation = transform->translation;
	// 親のワールド行列（親がいなければ単位行列）の逆行列
	const Entity* owner = EntityManager::Get(entity);
	const TransformComponent* parent = (owner != nullptr) ? EntityManager::Get<TransformComponent>(owner->parent) : nullptr;
	drag.parentInverse = (parent != nullptr) ? Inverse(parent->worldMatrix) : MakeIdentity4x4();
	drag.startMouse = ImGui::GetMousePos();
}

// 押している間：押した所からマウスが動いた分だけ、Translation を動かす
void UpdateDrag(const ImVec2& imageSize) {
	TransformComponent* transform = EntityManager::Get<TransformComponent>(drag.entity);
	if (transform == nullptr) {
		drag = {}; // ドラッグ中に消された（Undo など）
		return;
	}
	// 少し動かすまでは動かさない（IsMouseDragging：押した所から決まった距離より動いたか。ImGui の io.MouseDragThreshold）
	if (!drag.isDragging && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		return;
	}
	drag.isDragging = true;
	// --- マウスが動いた量（画面のピクセル）→ 1280×720 の画面のピクセル（UI の位置の単位。Y は上が＋）---
	const ImVec2 mouse = ImGui::GetMousePos();
	const Vector3 worldMove = {
	    (mouse.x - drag.startMouse.x) / imageSize.x * Renderer::kSpriteBaseWidth,
	    -(mouse.y - drag.startMouse.y) / imageSize.y * Renderer::kSpriteBaseHeight,
	    0.0f,
	};
	// --- 親から見た量に直す（親が回っている・大きくしてある物も、マウスについてくるように）---
	// 動かした量は「向き」なので、点の変換の差（移動の分が引かれて消える）で、回転と大きさだけを戻す
	const Vector3 localMove = TransformPoint(worldMove, drag.parentInverse) - TransformPoint({0.0f, 0.0f, 0.0f}, drag.parentInverse);
	// 1 ピクセルずつ（Inspector の数字が 150.37 のようにならないように）。Z はそのまま
	transform->translation = {
	    std::round(drag.startTranslation.x + localMove.x),
	    std::round(drag.startTranslation.y + localMove.y),
	    drag.startTranslation.z,
	};
}

// 離した：動かしていたら、押す前と後を1件の履歴にする（Inspector で値を変えたときと同じ入口）
void EndPress() {
	const ComponentEditor* editor = ComponentEditorRegistry::Find<TransformComponent>();
	const TransformComponent* transform = EntityManager::Get<TransformComponent>(drag.entity);
	if (drag.isDragging && editor != nullptr && transform != nullptr) {
		EditorHistory::RecordComponentChange(drag.entity, *editor, drag.before, editor->Save(transform));
		EditorHistory::CommitComponentChanges();
	}
	drag = {};
}

//=============================================================================
// 枠を描く
//=============================================================================
void DrawShape(ImDrawList* drawList, const UIShape& shape, const ImVec2& imageMin, const ImVec2& imageSize, ImU32 color, bool drawPivot) {
	const auto toImage = [&imageMin, &imageSize](const Vector2& ndc) { return ToImage(ndc, imageMin, imageSize); };
	// 左下 → 右下 → 右上 → 左上 の順に回る四角（回っていても、そのままの形で描ける）
	drawList->AddQuad(toImage(shape.quad.lb), toImage(shape.quad.rb), toImage(shape.quad.rt), toImage(shape.quad.lt), color, kOutlineThickness);
	if (drawPivot) {
		drawList->AddCircle(toImage(shape.position), kPivotRadius, color, 0, kOutlineThickness);
	}
}
} // namespace

//=============================================================================
// 描画
//=============================================================================
bool UIGizmo::Draw(Handle<Entity> selected, const ImVec2& imageMin, const ImVec2& imageSize, bool canUseMouse) {
	// --- 押している間は動かす。離したら履歴に積む（マウスが絵の外へ出ても、離すまで続ける）---
	if (drag.isPressed) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			UpdateDrag(imageSize);
		} else {
			EndPress();
		}
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true); // 絵の外（黒い余白）には描かない
	// --- マウスの下の UI（Pick UI がオンのときだけ。押している間は探さない）---
	const bool canPick = EditorViewport::GetViewFlags().scenePickUI && canUseMouse && !drag.isPressed;
	const Handle<Entity> hovered = canPick ? PickUI(ToNdc(ImGui::GetMousePos(), imageMin, imageSize)) : Handle<Entity>{};
	UIShape shape;
	if (hovered.IsValid() && hovered != selected && CalcShape(hovered, shape)) {
		DrawShape(drawList, shape, imageMin, imageSize, kHoveredColor, false);
	}
	// --- 選んでいる UI の枠と、位置（Pivot の所）の印 ---
	if (CalcShape(selected, shape)) {
		DrawShape(drawList, shape, imageMin, imageSize, kSelectedColor, true);
	}
	drawList->PopClipRect();

	// --- クリック：UI の上なら、それを選んで、ドラッグで動かせるようにする ---
	if (hovered.IsValid() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		HierarchyWindow::SelectAndReveal(hovered); // Hierarchy の一覧でも見えるようにする
		BeginPress(hovered);
		return true;
	}
	return drag.isPressed;
}

bool UIGizmo::IsScreenUI(Handle<Entity> entity) { return EntityManager::Get<ImageComponent>(entity) != nullptr || EntityManager::Get<TextComponent>(entity) != nullptr; }
#endif