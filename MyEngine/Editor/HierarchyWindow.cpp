#include "HierarchyWindow.h"

#include <externals/imgui/imgui.h>

#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
Handle<Entity> HierarchyWindow::selected_;
Handle<Entity> HierarchyWindow::createChildOf_;
Handle<Entity> HierarchyWindow::destroyRequest_;
Handle<Entity> HierarchyWindow::reparentChild_;
Handle<Entity> HierarchyWindow::reparentParent_;
bool HierarchyWindow::createRootRequest_ = false;
bool HierarchyWindow::reparentRequest_ = false;

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

	// Handleの中身をIDにする（名前が同じEntityがあってもぶつからない）
	ImGui::PushID(static_cast<int>(handle.index));
	bool isOpen = ImGui::TreeNodeEx("##node", flags, "%s", entity->name.c_str());

	// --- クリックで選択 ---
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		selected_ = handle;
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
		if (ImGui::MenuItem("Create Child")) {
			createChildOf_ = handle;
		}
		if (ImGui::MenuItem("Destroy")) {
			destroyRequest_ = handle;
		}
		ImGui::EndPopup();
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
// 一覧を描き終わってから操作を反映する
//=============================================================================
void HierarchyWindow::ApplyRequests() {
	// --- 作成 ---
	if (createRootRequest_) {
		selected_ = EntityManager::Create("Entity");
		createRootRequest_ = false;
	}
	if (createChildOf_.IsValid()) {
		selected_ = EntityManager::Create("Child", createChildOf_);
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
		EntityManager::Destroy(destroyRequest_);
		destroyRequest_ = Handle<Entity>{};
	}
}