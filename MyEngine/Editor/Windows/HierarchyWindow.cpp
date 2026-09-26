#include "HierarchyWindow.h"

#include <algorithm>
#include <cfloat>
#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/History/EditorHistory.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Light/LightComponent.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::renaming_;
std::string HierarchyWindow::renameBuffer_;
bool HierarchyWindow::renameFocusRequest_ = false;

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン

// 親のHandle（いなければ無効なHandle＝root）
Handle<Entity> ParentOf(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	return entity ? entity->parent : Handle<Entity>{};
}

// 子がいるか（いなければ矢印を出さない）
bool HasChild(Handle<Entity> handle) {
	for (const Entity& entity : EntityManager::GetAll()) {
		if (entity.parent == handle) {
			return true;
		}
	}
	return false;
}

// ドロップされたEntityを受け取る。newParentの子にする（無効ならrootへ）
void AcceptEntityDrop(Handle<Entity> newParent) {
	if (!ImGui::BeginDragDropTarget()) {
		return;
	}
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HierarchyWindow::kDragDropType)) {
		EditorHistory::RequestReparent(*static_cast<const Handle<Entity>*>(payload->Data), newParent);
	}
	ImGui::EndDragDropTarget();
}
} // namespace


//=============================================================================
// 描画
//=============================================================================
void HierarchyWindow::Draw() {
	// Undoなどで消えたEntityを持ち越さない
	if (!EntityManager::IsAlive(selected_)) {
		selected_ = {};
	}
	if (!EntityManager::IsAlive(renaming_)) {
		renaming_ = {};
		renameFocusRequest_ = false;
	}

	if (ImGui::Begin("Hierarchy")) {
		DrawToolbar();
		ImGui::Separator();

		// --- 一覧（rootから入れ子で描く）---
		// 作成・削除などはEditorHistoryに予約するだけなので、描いている途中で一覧が変わることはない
		for (const Entity& entity : EntityManager::GetAll()) {
			if (!entity.parent.IsValid()) {
				DrawEntityNode(entity.self);
			}
		}
		DrawEmptySpace();
		HandleShortcuts();
	}
	ImGui::End();
}

//=============================================================================
// 見出し
//=============================================================================
void HierarchyWindow::DrawToolbar() {
	const float buttonSize = ImGui::GetFrameHeight();
	const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x; // この行の右端（ウィンドウの中での位置）

	ImGui::AlignTextToFramePadding(); // 文字の高さをボタンに合わせる
	ImGui::Text("Entities (%zu)", EntityManager::GetCount());

	// --- 右端に＋ボタン。押すと作成メニュー ---
	ImGui::SameLine(right - buttonSize);
	if (EditorWidgets::PlusButton("##create", ImVec2(buttonSize, buttonSize))) {
		ImGui::OpenPopup("createMenu");
	}
	ImGui::SetItemTooltip("Create");
	if (ImGui::BeginPopup("createMenu")) {
		DrawCreateMenu();
		ImGui::EndPopup();
	}
}

//=============================================================================
// Entity1つ分
//=============================================================================
void HierarchyWindow::DrawEntityNode(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	const bool isRenaming = (handle == renaming_);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!isRenaming) {
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth; // 名前の変更中は、矢印の横に入力欄を置くので横幅いっぱいにしない
	}
	if (!HasChild(handle)) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (handle == selected_) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// EntityIdをIDにする（名前が同じEntityがあってもぶつからない）
	// Handleの番号だと、Play / Stop で作り直したときに振り直されて、開いた・閉じた状態が別のEntityに移ってしまう
	ImGui::PushID(static_cast<int>(entity->id));
	const bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", isRenaming ? "" : entity->name.c_str());

	if (isRenaming) {
		ImGui::SameLine();
		DrawRenameField(handle);
	} else {
		// --- クリックで選択（右クリックでも選ぶ）、ダブルクリックで名前の変更 ---
		// 左クリックは「離したとき、ドラッグしていなければ」選ぶ。押した瞬間に選ぶと、Inspectorの参照欄へドラッグする前に
		// Inspectorの中身がドラッグしているEntityに切り替わってしまい、落とす先が消える
		const bool clickedLeft = ImGui::IsItemDeactivated() && ImGui::GetDragDropPayload() == nullptr;
		const bool clicked = clickedLeft || ImGui::IsItemClicked(ImGuiMouseButton_Right);
		if (clicked && !ImGui::IsItemToggledOpen()) {
			selected_ = handle;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			StartRename(handle);
		}

		// --- ドラッグ（このEntityを運ぶ）・ドロップ（このEntityを親にする）---
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(kDragDropType, &handle, sizeof(handle));
			ImGui::TextUnformatted(entity->name.c_str());
			ImGui::EndDragDropSource();
		}
		AcceptEntityDrop(handle);

		// --- 右クリックのメニュー ---
		if (ImGui::BeginPopupContextItem("entityMenu")) {
			DrawEntityMenu(handle);
			ImGui::EndPopup();
		}
	}

	// --- 子を描く ---
	if (isOpen) {
		for (const Entity& other : EntityManager::GetAll()) {
			if (other.parent == handle) {
				DrawEntityNode(other.self);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

//=============================================================================
// 一覧の下の何も無い所
//=============================================================================
void HierarchyWindow::DrawEmptySpace() {
	// 残りの高さいっぱいに透明なボタンを置いて、クリック・ドロップ・右クリックを受ける
	const ImVec2 space = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##emptySpace", ImVec2((std::max)(space.x, 1.0f), (std::max)(space.y, ImGui::GetFrameHeight())));
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		selected_ = {}; // 何も無い所をクリックしたら選択を外す
	}
	AcceptEntityDrop({}); // ここへ落としたらrootへ移す
	if (ImGui::BeginPopupContextItem("emptySpaceMenu")) {
		DrawCreateMenu();
		ImGui::EndPopup();
	}
}

//=============================================================================
// メニュー
//=============================================================================
void HierarchyWindow::DrawEntityMenu(Handle<Entity> handle) {
	if (ImGui::MenuItem("Rename", "F2")) {
		StartRename(handle);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Copy", "Ctrl+C")) {
		EditorHistory::Copy(handle);
	}
	// Paste：このEntityと同じ親の下（兄弟）に貼る　Paste As Child：このEntityの子として貼る
	if (ImGui::MenuItem("Paste", "Ctrl+V", false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste(ParentOf(handle));
	}
	if (ImGui::MenuItem("Paste As Child", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste(handle);
	}
	if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
		EditorHistory::RequestDuplicate(handle);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Create Child")) {
		EditorHistory::RequestCreate("Child", handle);
	}
	if (ImGui::MenuItem("Delete", "Del")) {
		EditorHistory::RequestDestroy(handle);
	}
}

void HierarchyWindow::DrawCreateMenu() {
	if (ImGui::MenuItem("Create Entity")) {
		EditorHistory::RequestCreate("Entity", {});
	}
	if (ImGui::MenuItem("Create Child", nullptr, false, selected_.IsValid())) {
		EditorHistory::RequestCreate("Child", selected_);
	}
	// --- ライト：Componentを付けた状態で作る（1回のUndoでEntityごと消える）---
	if (ImGui::BeginMenu("Light")) {
		if (ImGui::MenuItem("Directional Light")) {
			TransformComponent transform;
			transform.translation = {0.0f, 3.0f, 0.0f};                         // 位置は照らし方に関係ない（ギズモを描く場所）
			transform.rotation = {50.0f * kDegToRad, -30.0f * kDegToRad, 0.0f}; // 斜め上から照らす（Unityの最初のシーンと同じ位置・向き）
			EditorHistory::RequestCreate("Directional Light", {}, {ComponentSnapshot::Make(transform), ComponentSnapshot::Make(DirectionalLightComponent{})});
		}
		if (ImGui::MenuItem("Point Light")) {
			EditorHistory::RequestCreate("Point Light", {}, {ComponentSnapshot::Make(PointLightComponent{})});
		}
		if (ImGui::MenuItem("Spot Light")) {
			TransformComponent transform;
			transform.rotation.x = 90.0f * kDegToRad; // 真下を照らす（前＝+Zを、X軸で90度倒す）
			EditorHistory::RequestCreate("Spot Light", {}, {ComponentSnapshot::Make(transform), ComponentSnapshot::Make(SpotLightComponent{})});
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	// 何も無い所から貼るので、一番上（root）に貼る
	if (ImGui::MenuItem("Paste", nullptr, false, EditorHistory::CanPaste())) {
		EditorHistory::RequestPaste({});
	}
}

//=============================================================================
// ショートカット（Hierarchyを触っているときだけ）
//=============================================================================
void HierarchyWindow::HandleShortcuts() {
	// 名前や数値を入力中なら、入力欄のほうの操作を優先する
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || renaming_.IsValid() || ImGui::GetIO().WantTextInput) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
		StartRename(selected_); // 選んでいなければ何もしない
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && selected_.IsValid()) {
		EditorHistory::RequestDestroy(selected_);
	}
	// IsKeyChordPressed：修飾キーまで完全に一致したときだけ（Ctrl+Shift+Cでは反応しない）
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C) && selected_.IsValid()) {
		EditorHistory::Copy(selected_);
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_V)) {
		EditorHistory::RequestPaste(ParentOf(selected_)); // 選んでいるEntityの兄弟に貼る（選んでいなければroot）
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D) && selected_.IsValid()) {
		EditorHistory::RequestDuplicate(selected_);
	}
}

//=============================================================================
// 名前の変更を始める
//=============================================================================
void HierarchyWindow::StartRename(Handle<Entity> handle) {
	const Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}
	renaming_ = handle;
	renameBuffer_ = entity->name; // 今の名前から編集を始める
	renameFocusRequest_ = true;
	selected_ = handle;
}

//=============================================================================
// 名前の入力欄
//=============================================================================
void HierarchyWindow::DrawRenameField(Handle<Entity> handle) {
	// 出した最初のフレームだけ、入力欄にフォーカスを移す（すぐに文字を打てるように）
	const bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全選択（そのまま打つと置き換わる）
	const bool entered = EditorWidgets::InputText("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	// --- 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）---
	// Escで抜けたときはImGuiが中身を元に戻すので、名前は変わらない＝取り消しになる
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		EditorHistory::RequestRename(handle, renameBuffer_); // 空・同じ名前なら何もしない
	}

	// --- 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる ---
	bool finished = entered || ImGui::IsItemDeactivated();
	// 入力欄が有効にならないまま別の所をクリックされたときも閉じる（出しっぱなしにしない）
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_ = {};
	}
}