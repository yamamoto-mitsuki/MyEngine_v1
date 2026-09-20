#include "InspectorWindow.h"

#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <string_view>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Inspector/LightEditor.h"
#include "MyEngine/Editor/Inspector/ModelRendererEditor.h"
#include "MyEngine/Editor/Inspector/TransformEditor.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

namespace {
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）

// Add Componentに出すか（外せる種類で、検索にも合う）
bool IsListed(const ComponentEditor& editor, const ImGuiTextFilter& filter) { return editor.IsOptional() && filter.PassFilter(editor.GetName()); }

// カテゴリのパスの depth 段目の名前（"Gameplay/Movement" の1段目は "Movement"）。そこで終わっていれば空
std::string_view CategorySegment(std::string_view path, size_t depth) {
	size_t start = 0;
	for (size_t i = 0; i < depth; ++i) {
		const size_t slash = path.find('/', start);
		if (slash == std::string_view::npos) {
			return {}; // これより下の段は無い
		}
		start = slash + 1;
	}
	const size_t slash = path.find('/', start);
	return path.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
}

// Componentを1つ、押せる項目として出す
void DrawAddComponentItem(Handle<Entity> handle, const ComponentEditor& editor) {
	// すでに持っている・追加を予約済みなら、灰色にして押せなくする
	ImGui::BeginDisabled(editor.Get(handle) || editor.IsAddPending(handle));
	if (ImGui::Selectable(editor.GetName())) { // Selectableを押すとポップアップは自動で閉じる
		EditorHistory::RequestAddComponent(handle, editor);
	}
	ImGui::EndDisabled();
}

// [begin, end) は同じ上位カテゴリの並び。depth段目の名前で切って、入れ子のメニューにする
void DrawAddComponentLevel(Handle<Entity> handle, const std::vector<const ComponentEditor*>& items, size_t begin, size_t end, size_t depth, const ImGuiTextFilter& filter) {
	size_t index = begin;

	// パスがこの段で終わっている物（"Gameplay" 直下など）を先に並べる。並びの決まりで、これらは必ず先頭に集まっている
	while (index < end && CategorySegment(items[index]->GetCategory(), depth).empty()) {
		DrawAddComponentItem(handle, *items[index]);
		++index;
	}

	// 残りを、同じ名前のかたまりごとに入れ子にする
	while (index < end) {
		const std::string name(CategorySegment(items[index]->GetCategory(), depth));
		size_t groupEnd = index;
		while (groupEnd < end && CategorySegment(items[groupEnd]->GetCategory(), depth) == name) {
			++groupEnd;
		}
		// 検索中は、見つかったカテゴリを全部開いて見せる
		if (filter.IsActive()) {
			ImGui::SetNextItemOpen(true);
		}
		if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
			DrawAddComponentLevel(handle, items, index, groupEnd, depth + 1, filter);
			ImGui::TreePop();
		}
		index = groupEnd;
	}
}
} // namespace


//=============================================================================
// 初期化
//=============================================================================
void InspectorWindow::Initialize() {
	// エンジンのComponent。Inspectorの区画はカテゴリ順（同じカテゴリの中は登録した順）に並ぶ
	ComponentEditorRegistry::Register(std::make_unique<TransformEditor>());
	ComponentEditorRegistry::Register(std::make_unique<ModelRendererEditor>());
	ComponentEditorRegistry::Register(std::make_unique<DirectionalLightEditor>());
	ComponentEditorRegistry::Register(std::make_unique<PointLightEditor>());
	ComponentEditorRegistry::Register(std::make_unique<SpotLightEditor>());
}

//=============================================================================
// 描画
//=============================================================================
void InspectorWindow::Draw() {
	if (ImGui::Begin("Inspector")) {
		const Handle<Entity> handle = HierarchyWindow::GetSelected();
		if (EntityManager::IsAlive(handle)) {
			DrawHeader(handle);
			for (const std::unique_ptr<ComponentEditor>& editor : ComponentEditorRegistry::GetAll()) {
				DrawComponent(handle, *editor);
			}
			DrawAddComponent(handle);
		} else {
			ImGui::TextDisabled("No entity selected");
		}
	}
	ImGui::End();

	// どの項目も操作していない（マウスを離した・入力を確定した）なら、ここまでの編集を1件の履歴にまとめる
	if (!ImGui::IsAnyItemActive()) {
		EditorHistory::CommitComponentChanges();
	}
}

//=============================================================================
// Entityそのものの情報
//=============================================================================
void InspectorWindow::DrawHeader(Handle<Entity> handle) {
	// ポインタは使い捨てにする（ARCHITECTURE.md 原則1）。持ち越すのはHandleだけ
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// --- 有効・無効（直接書き換えず、履歴を通す。反映は次のフレームの頭）---
	bool isActive = entity->isActive;
	if (ImGui::Checkbox("##isActive", &isActive)) {
		EditorHistory::RequestSetActive(handle, isActive);
	}
	ImGui::SameLine();

	// --- 名前（変更はHierarchyのダブルクリック・F2から）---
	ImGui::TextUnformatted(entity->name.c_str());

	// --- 参考情報（読み取り専用）---
	ImGui::TextDisabled("ID: %llu  Handle: %u / %u", entity->id, handle.index, handle.generation);
	if (const Entity* parent = EntityManager::Get(entity->parent)) {
		ImGui::TextDisabled("Parent: %s", parent->name.c_str());
	} else {
		ImGui::TextDisabled("Parent: (root)");
	}
	ImGui::Separator();
}

//=============================================================================
// Component1つ分の区画
//=============================================================================
void InspectorWindow::DrawComponent(Handle<Entity> handle, const ComponentEditor& editor) {
	void* component = editor.Get(handle);
	if (!component) {
		return; // このEntityは持っていない
	}

	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（"Position" などの項目名がComponent同士でぶつからないように）
	ImGui::PushID(editor.GetName());
	const bool isOpen = ImGui::CollapsingHeader(editor.GetName(), ImGuiTreeNodeFlags_DefaultOpen);

	// --- 見出しの右クリック ---
	if (ImGui::BeginPopupContextItem("componentMenu")) {
		if (ImGui::MenuItem("Remove Component", nullptr, false, editor.IsOptional())) {
			EditorHistory::RequestRemoveComponent(handle, editor);
		}
		ImGui::EndPopup();
	}

	if (isOpen) {
		// 描く前の中身を写しておき、描いた後と比べる。変わった所だけがUndoの対象になる
		// （Componentごとの項目を1つずつ書き並べなくてよいので、Componentが増えてもここは変えない）
		const auto* bytes = static_cast<const std::byte*>(component);
		const std::vector<std::byte> before(bytes, bytes + editor.GetSize());
		editor.Draw(component);
		EditorHistory::RecordComponentChange(handle, editor, before.data(), component);
	}
	ImGui::PopID();
}

//=============================================================================
// Componentを足すボタン
//=============================================================================
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	ImGui::Spacing();

	// Componentの見出し（CollapsingHeader）と同じ色の、横幅いっぱいのボタンにする
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
	const bool clicked = EditorWidgets::PlusButton("Add Component", ImVec2(-FLT_MIN, 0.0f));
	ImGui::PopStyleColor(3);
	if (clicked) {
		ImGui::OpenPopup("addComponent");
	}

	// --- ボタンの真下に、ボタンと同じ幅の縦長のポップアップを出す ---
	const ImVec2 buttonMin = ImGui::GetItemRectMin();
	const ImVec2 buttonMax = ImGui::GetItemRectMax();
	const float width = buttonMax.x - buttonMin.x;
	ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y));
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, kAddComponentMaxHeight));
	if (!ImGui::BeginPopup("addComponent")) {
		return;
	}

	// --- 検索欄（開いた瞬間に文字を打てるようにする）---
	static ImGuiTextFilter filter; // ポップアップは同時に1つしか開かないので、関数の中のstaticで持つ
	if (ImGui::IsWindowAppearing()) {
		filter.Clear();
		ImGui::SetKeyboardFocusHere();
	}
	filter.Draw("##search", -FLT_MIN);
	ImGui::Separator();

	// --- カテゴリ（押すと下に開く）→ その中のComponent ---
	// 登録表はカテゴリ順に並んでいるので、同じカテゴリの物は必ず続いて並んでいる（だから範囲で切って入れ子にできる）
	std::vector<const ComponentEditor*> items;
	for (const std::unique_ptr<ComponentEditor>& editor : ComponentEditorRegistry::GetAll()) {
		if (IsListed(*editor, filter)) {
			items.push_back(editor.get());
		}
	}
	DrawAddComponentLevel(handle, items, 0, items.size(), 0, filter);

	ImGui::EndPopup();
}