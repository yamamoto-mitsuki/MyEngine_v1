#include "InspectorWindow.h"

#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include <externals/imgui/imgui.h>
#include <externals/magic_enum/magic_enum.hpp>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Inspector/ComponentEditor.h"
#include "MyEngine/Editor/Inspector/LightEditor.h"
#include "MyEngine/Editor/Inspector/ModelRendererEditor.h"
#include "MyEngine/Editor/Inspector/TransformEditor.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Editor/Windows/HierarchyWindow.h"
#include "MyEngine/ECS/EntityManager.h"
#include "MyEngine/String/ConvertString.h"

namespace {
constexpr float kAddComponentMaxHeight = 420.0f; // Add Componentのポップアップの高さの上限（超えたらスクロール）

// Add Componentに出すか（外せる種類で、検索にも合う）
bool IsListed(const ComponentEditor& editor, const ImGuiTextFilter& filter) { return editor.IsOptional() && filter.PassFilter(editor.GetName()); }

// コンパイラが入れたファイルの場所（C:\...\Source\Components\Spin.cpp）を、今の作業フォルダ（ゲームのプロジェクトのフォルダ）からの相対パスに縮める
// 別のドライブなどで縮められなければ、そのまま返す
std::string ShortenSourcePath(const char* file) {
	std::error_code error; // 例外ではなくエラーコードで受ける
	const std::filesystem::path relative = std::filesystem::relative(file, error);
	return (error || relative.empty()) ? std::string(file) : relative.string();
}

// 書いてあるファイルを、.cpp を開くアプリ（Windows の「既定のアプリ」。ふつうは Visual Studio）で開く
// file はコンパイラが入れた UTF-8 の文字なので、Windows に渡す前にワイド文字にする
void OpenSourceFile(const char* file) { ShellExecuteW(nullptr, L"open", ConvertString(std::string(file)).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

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
	if (ImGui::Selectable(editor.GetName())) {
		EditorHistory::RequestAddComponent(handle, editor);
		// 一覧はスクロールするための子ウィンドウの中なので、Selectable を押してもポップアップは自動では閉じない。ここで閉じる
		ImGui::CloseCurrentPopup();
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

	// --- 見出しにマウスを乗せると、書いてあるファイルを出す（ゲームのComponentはどこに置いてもよいので、探さなくて済むように）---
	if (const char* file = editor.GetSourceFile(); file != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
		ImGui::SetTooltip("%s", ShortenSourcePath(file).c_str());
	}

	// --- 見出しの右クリック ---
	if (ImGui::BeginPopupContextItem("componentMenu")) {
		if (ImGui::MenuItem("Remove Component", nullptr, false, editor.IsOptional())) {
			EditorHistory::RequestRemoveComponent(handle, editor);
		}
		// 書いてあるファイルを開く
		if (const char* file = editor.GetSourceFile(); file != nullptr && ImGui::MenuItem("Edit Script")) {
			OpenSourceFile(file);
		}
		ImGui::EndPopup();
	}

	if (isOpen) {
		// 描く前の中身を写しておき、描いた後と比べる。変わった所だけがUndoの対象になる
		// （写し方は editor が決める。Componentごとの項目を1つずつ書き並べなくてよいので、Componentが増えてもここは変えない）
		const std::vector<std::byte> before = editor.Save(component);
		editor.Draw(component);
		EditorHistory::RecordComponentChange(handle, editor, before, editor.Save(component));
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

	// --- ボタンと同じ幅の縦長のポップアップを、ボタンの下に入れば下、入らなければ上に出す ---
	// （ボタンが画面の下の方にあると、下に出したポップアップが画面の外にはみ出して、下の方の項目が選べなかった）
	const ImVec2 buttonMin = ImGui::GetItemRectMin();
	const ImVec2 buttonMax = ImGui::GetItemRectMax();
	const float width = buttonMax.x - buttonMin.x;
	const ImGuiViewport* viewport = ImGui::GetWindowViewport(); // エディタ全体の画面（メニューバーの下から）
	const float margin = ImGui::GetStyle().WindowPadding.y;     // 画面の端から少し離す
	const float spaceBelow = viewport->WorkPos.y + viewport->WorkSize.y - buttonMax.y - margin;
	const float spaceAbove = buttonMin.y - viewport->WorkPos.y - margin;
	const bool opensBelow = (spaceBelow >= kAddComponentMaxHeight) || (spaceBelow >= spaceAbove); // 下に全部入る、または下の方が広い
	const float maxHeight = (std::min)(kAddComponentMaxHeight, opensBelow ? spaceBelow : spaceAbove);
	if (opensBelow) {
		ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMax.y)); // 左上をボタンの左下に
	} else {
		ImGui::SetNextWindowPos(ImVec2(buttonMin.x, buttonMin.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f)); // 左下をボタンの左上に（カテゴリを開くと上へ伸びる）
	}
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, maxHeight));
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
	
	// 一覧だけを子ウィンドウに入れて、スクロールさせる（検索欄はいつも見える）
	// AutoResizeY：中身に合わせて高さが決まる。上限（ポップアップの残りの高さ）を超えたらスクロールバーが出る
	const float listMaxHeight = (std::max)(maxHeight - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y, ImGui::GetFrameHeight());
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, listMaxHeight));
	if (ImGui::BeginChild("##componentList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY)) {
		DrawAddComponentLevel(handle, items, 0, items.size(), 0, filter);
	}
	ImGui::EndChild();

	ImGui::EndPopup();
}