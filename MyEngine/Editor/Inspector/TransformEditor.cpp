#include "TransformEditor.h"
#include <externals/imgui/imgui.h>
#include "MyEngine/ECS/EntityManager.h"

void TransformEditor::DrawComponent(TransformComponent& transform) const {
	ImGui::DragFloat3("Position", &transform.translation.x, 0.05f);

	// 回転は中ではクォータニオンで持っているので、見せるときだけオイラー角（度）に直す（GetEulerAngles：最後に決めた角度に一番近い表し方）
	// 何も触っていないフレームは、Component を書き換えない（書き換えると、Undo の対象に入ってしまう）
	// 角度は度のまま持っているので、触っていない軸の数字は1文字も変わらない（前はラジアンとの往復で、少しずれることがあった）
	Vector3 euler = GetEulerAngles(transform);
	if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f)) {
		SetEulerAngles(transform, euler);
	}

	ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);

	if (ImGui::Button("Reset")) {
		transform.translation = {0.0f, 0.0f, 0.0f};
		SetEulerAngles(transform, {0.0f, 0.0f, 0.0f});
		transform.scale = {1.0f, 1.0f, 1.0f};
	}
	// --- 計算結果（読み取り専用）---
	// EntityManager::UpdateTransformsが作った、親の行列まで掛けた結果。親子が繋がっているかの確認に使う
	const Matrix4x4& world = transform.worldMatrix;
	ImGui::TextDisabled("World Position: %.3f, %.3f, %.3f", world.m[3][0], world.m[3][1], world.m[3][2]);
	const Quaternion& q = transform.rotation; // 中に入っている回転そのもの（x・y・z は 回す軸 × sin(角度/2)、w は cos(角度/2)）
	ImGui::TextDisabled("Quaternion: %.3f, %.3f, %.3f, %.3f", q.x, q.y, q.z, q.w);
	if (ImGui::TreeNode("World Matrix")) {
		for (int row = 0; row < 4; ++row) {
			ImGui::Text("%8.3f %8.3f %8.3f %8.3f", world.m[row][0], world.m[row][1], world.m[row][2], world.m[row][3]);
		}
		ImGui::TreePop();
	}
}