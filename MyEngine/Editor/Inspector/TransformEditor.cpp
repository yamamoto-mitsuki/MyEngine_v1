#include "TransformEditor.h"

#include <numbers>

#include <externals/imgui/imgui.h>

#include "MyEngine/Entity/EntityManager.h"

namespace {
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>; // ラジアン → 度
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f; // 度 → ラジアン
} // namespace

TransformComponent* TransformEditor::GetComponent(Handle<Entity> handle) const { return EntityManager::GetTransform(handle); }

void TransformEditor::DrawComponent(TransformComponent& transform) const {
	ImGui::DragFloat3("Position", &transform.translation.x, 0.05f);

	// 回転は中ではラジアンで持っているので、見せるときだけ度に直す。
	// 動かした軸だけ書き戻す（触っていない軸まで度↔ラジアンを往復させると、値がわずかにずれてUndoの対象にも入ってしまう）
	float* radians = &transform.rotation.x;
	const float degrees[3] = {radians[0] * kRadToDeg, radians[1] * kRadToDeg, radians[2] * kRadToDeg};
	float edited[3] = {degrees[0], degrees[1], degrees[2]};
	if (ImGui::DragFloat3("Rotation", edited, 0.5f)) {
		for (int axis = 0; axis < 3; ++axis) {
			if (edited[axis] != degrees[axis]) {
				radians[axis] = edited[axis] * kDegToRad;
			}
		}
	}

	ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform.translation = {0.0f, 0.0f, 0.0f};
		transform.rotation = {0.0f, 0.0f, 0.0f};
		transform.scale = {1.0f, 1.0f, 1.0f};
	}

	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform.worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}
}