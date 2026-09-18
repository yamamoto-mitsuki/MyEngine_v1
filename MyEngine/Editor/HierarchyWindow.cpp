#include "HierarchyWindow.h"

#include <cfloat>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::createChildOf_;
Handle<Entity> HierarchyWindow::destroyRequest_;
Handle<Entity> HierarchyWindow::reparentChild_;
Handle<Entity> HierarchyWindow::reparentParent_;
bool HierarchyWindow::createRootRequest_ = false;
bool HierarchyWindow::reparentRequest_ = false;
Handle<Entity> HierarchyWindow::renaming_;
std::string HierarchyWindow::renameBuffer_;
bool HierarchyWindow::renameFocusRequest_ = false;

namespace {
constexpr const char* kDragDropType = "ENTITY_HANDLE"; // ドラッグで運ぶものの種類の名前
}


//=============================================================================
// 描画
//=============================================================================
void HierarchyWindow::Draw() {
	ImGui::Begin("Hierarchy");

	// --- 作成・削除 ---
	if (ImGui::Button("Create Entity")) {
		createRootRequest_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Destroy Selected")) {
		destroyRequest_ = selected_;
	}
	ImGui::Text("Count: %zu", EntityManager::GetCount());
	ImGui::Separator();

	// --- 一覧（rootから入れ子で描く） ---
	// 一覧を描いている間にEntityを作ったり消したりすると、並びが変わって崩れる。
	// なので操作は覚えておくだけにして、描き終わってから ApplyRequests で反映する
	for (const Entity& entity : EntityManager::GetAll()) {
		if (!entity.parent.IsValid()) {
			DrawEntityNode(entity.self);
		}
	}

	// --- F2で、選択中のEntityの名前を変更（Windowsのエクスプローラと同じ）---
	bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (isFocused && !renaming_.IsValid() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
		StartRename(selected_);
	}

	// --- 何も無い所へドロップしたら、rootへ移す ---
	ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 12.0f)); // ドロップを受ける余白
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropType)) {
			reparentChild_ = *static_cast<const Handle<Entity>*>(payload->Data);
			reparentParent_ = Handle<Entity>{}; // 親なし＝root
			reparentRequest_ = true;
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();

	ApplyRequests();
}


//=============================================================================
// Entity1つ分
//=============================================================================
void HierarchyWindow::DrawEntityNode(Handle<Entity> handle) {
	Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// 子がいるかを先に調べる（いなければ矢印を出さない）
	bool hasChild = false;
	for (const Entity& other : EntityManager::GetAll()) {
		if (other.parent == handle) {
			hasChild = true;
			break;
		}
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!hasChild) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (handle == selected_) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// 名前を変更中は、ラベルを消して、矢印の横に入力欄を出す
	bool isRenaming = (handle == renaming_);
	if (isRenaming) {
		flags &= ~ImGuiTreeNodeFlags_SpanAvailWidth; // 横幅いっぱいのままだと、入力欄が右端へ押し出される
	}

	// Handleの中身をIDにする（名前が同じEntityがあってもぶつからない）
	ImGui::PushID(static_cast<int>(handle.index));
	bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", isRenaming ? "" : entity->name.c_str());

	if (isRenaming) {
		ImGui::SameLine();
		DrawRenameField(handle);
	} else {
		// --- クリックで選択、ダブルクリックで名前の変更 ---
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			selected_ = handle;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			StartRename(handle);
		}

		// --- ドラッグ（このEntityを運ぶ） ---
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(kDragDropType, &handle, sizeof(handle));
			ImGui::Text("%s", entity->name.c_str());
			ImGui::EndDragDropSource();
		}
		// --- ドロップ（このEntityを親にする） ---
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragDropType)) {
				reparentChild_ = *static_cast<const Handle<Entity>*>(payload->Data);
				reparentParent_ = handle;
				reparentRequest_ = true;
			}
			ImGui::EndDragDropTarget();
		}

		// --- 右クリックのメニュー ---
		if (ImGui::BeginPopupContextItem("context")) {
			if (ImGui::MenuItem("Rename", "F2")) {
				StartRename(handle);
			}
			if (ImGui::MenuItem("Create Child")) {
				createChildOf_ = handle;
			}
			if (ImGui::MenuItem("Destroy")) {
				destroyRequest_ = handle;
			}
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
	bool justStarted = renameFocusRequest_;
	if (renameFocusRequest_) {
		ImGui::SetKeyboardFocusHere();
		renameFocusRequest_ = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	// EnterReturnsTrue：Enterで確定　AutoSelectAll：最初は全選択（そのまま打つと置き換わる）
	bool entered = EditorWidgets::InputText("##rename", renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	// --- 確定：Enter、または欄の外をクリックして抜けた（書き換えていた場合）---
	// Escで抜けたときはImGuiが中身を元に戻すので、確定しても元の名前のまま＝取り消しになる
	if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
		Entity* entity = EntityManager::Get(handle);
		if (entity && !renameBuffer_.empty()) { // 空の名前にはしない（元の名前のまま）
			entity->name = renameBuffer_;
		}
	}

	// --- 終了：Enter・Esc・欄の外をクリック、のどれでも入力欄を閉じる ---
	bool finished = entered || ImGui::IsItemDeactivated();
	// 入力欄が有効にならないまま別の所をクリックされたときも閉じる（出しっぱなしにしない）
	if (!justStarted && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		finished = true;
	}
	if (finished) {
		renaming_ = Handle<Entity>{};
	}
}


//=============================================================================
// 一覧を描き終わってから操作を反映する
//=============================================================================
void HierarchyWindow::ApplyRequests() {
	// --- 作成（作ったらそのまま名前の変更を始める。Unityと同じ）---
	if (createRootRequest_) {
		StartRename(EntityManager::Create("Entity"));
		createRootRequest_ = false;
	}
	if (createChildOf_.IsValid()) {
		StartRename(EntityManager::Create("Child", createChildOf_));
		createChildOf_ = Handle<Entity>{};
	}
	// --- 親の付け替え ---
	if (reparentRequest_) {
		EntityManager::SetParent(reparentChild_, reparentParent_);
		reparentRequest_ = false;
		reparentChild_ = Handle<Entity>{};
		reparentParent_ = Handle<Entity>{};
	}
	// --- 削除（実際に消えるのはフレームの最後） ---
	if (destroyRequest_.IsValid()) {
		if (destroyRequest_ == selected_) {
			selected_ = Handle<Entity>{};
		}
		if (destroyRequest_ == renaming_) {
			renaming_ = Handle<Entity>{};
		}
		EntityManager::Destroy(destroyRequest_);
		destroyRequest_ = Handle<Entity>{};
	}
}