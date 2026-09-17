#include "InspectorWindow.h"

#include <cfloat>
#include <cstring>
#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/HierarchyWindow.h"
#include "MyEngine/Entity/EntityManager.h"

// 静的メンバ変数
char InspectorWindow::nameBuffer_[kNameBufferSize] = {};
Handle<Entity> InspectorWindow::nameBufferOwner_;

namespace {
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace


//=============================================================================
// 描画
//=============================================================================
void InspectorWindow::Draw() {
	ImGui::Begin("Inspector");

	// 選択中のEntityはHierarchyWindowが持っている
	Handle<Entity> handle = HierarchyWindow::GetSelected();
	if (!EntityManager::IsAlive(handle)) {
		ImGui::TextDisabled("No entity selected");
		ImGui::End();
		return;
	}

	// Componentを1つずつ区画にして描く。Componentが増えたらここに足していく
	DrawHeader(handle);
	ImGui::Separator();
	DrawTransform(handle);
	ImGui::Separator();
	DrawAddComponent(handle);

	ImGui::End();
}


//=============================================================================
// Entityそのものの情報
//=============================================================================
void InspectorWindow::DrawHeader(Handle<Entity> handle) {
	// ポインタは使い捨てにする（ARCHITECTURE.md 原則1）。持ち越すのはHandleだけ
	Entity* entity = EntityManager::Get(handle);
	if (!entity) {
		return;
	}

	// --- 有効・無効 ---
	ImGui::Checkbox("##isActive", &entity->isActive);
	ImGui::SameLine();

	// --- 名前 ---
	// 選択が変わったら、今の名前をバッファへ入れ直す
	if (nameBufferOwner_ != handle) {
		nameBufferOwner_ = handle;
		strncpy_s(nameBuffer_, entity->name.c_str(), kNameBufferSize - 1);
	}
	ImGui::SetNextItemWidth(-FLT_MIN); // 残りの幅いっぱいに広げる
	if (ImGui::InputText("##name", nameBuffer_, kNameBufferSize)) {
		entity->name = nameBuffer_;
	}

	// --- 参考情報（読み取り専用） ---
	ImGui::TextDisabled("Handle: index=%u generation=%u", handle.index, handle.generation);
	if (const Entity* parent = EntityManager::Get(entity->parent)) {
		ImGui::TextDisabled("Parent: %s", parent->name.c_str());
	} else {
		ImGui::TextDisabled("Parent: (root)");
	}
}


//=============================================================================
// TransformComponent
//=============================================================================
void InspectorWindow::DrawTransform(Handle<Entity> handle) {
	TransformComponent* transform = EntityManager::GetTransform(handle);
	if (!transform) {
		return;
	}

	// 全Entityが必ず持つ区画なので、開いた状態で始める
	if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	// CollapsingHeaderはIDの範囲を作らないので、自分で作る（Componentが増えたときに名前がぶつからないように）
	ImGui::PushID("Transform");

	ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);

	// 回転は中ではラジアンで持っているので、見せるときだけ度に直す。
	// 変わったときだけ書き戻す（毎フレーム度↔ラジアンを往復させると、値がじわじわずれる）
	Vector3 degrees = {transform->rotation.x * kRadToDeg, transform->rotation.y * kRadToDeg, transform->rotation.z * kRadToDeg};
	if (ImGui::DragFloat3("Rotation", &degrees.x, 0.5f)) {
		transform->rotation = {degrees.x * kDegToRad, degrees.y * kDegToRad, degrees.z * kDegToRad};
	}

	ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform->translation = {0.0f, 0.0f, 0.0f};
		transform->rotation = {0.0f, 0.0f, 0.0f};
		transform->scale = {1.0f, 1.0f, 1.0f};
	}

	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform->worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}


//=============================================================================
// Componentを足すボタン
//=============================================================================
void InspectorWindow::DrawAddComponent(Handle<Entity> handle) {
	if (!EntityManager::IsAlive(handle)) {
		return;
	}

	if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f))) {
		ImGui::OpenPopup("addComponent");
	}
	if (ImGui::BeginPopup("addComponent")) {
		// 足せるComponentが増えたら、ここにMenuItemを並べてEntityへ足す
		ImGui::TextDisabled("(none yet)");
		ImGui::EndPopup();
	}
}