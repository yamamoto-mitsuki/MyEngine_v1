#include "ModelRendererEditor.h"

#include <externals/imgui/imgui.h>

#include "MyEngine/Component/ComponentUI.h"
#include "MyEngine/Editor/Widgets/EditorWidgets.h"

//=============================================================================
// Inspectorの中身
//=============================================================================
void ModelRendererEditor::DrawComponent(ModelRendererComponent& render) const {
	ImGui::Checkbox("Enabled", &render.enabled);

	// --- どのモデル・テクスチャを使うか（resources以下のファイルから選ぶ。ComponentUIのアセット欄と同じ物）---
	ComponentUI& ui = GetInspectorUI();
	ui.AssetField("Model", render.modelHandle, AssetType::Model);
	ui.AssetField("Texture", render.textureHandle, AssetType::Texture, Tip("(none) なら、モデルのマテリアルのテクスチャを使う"));

	// --- 描画設定（enumはmagic_enumが名前を作る）---
	EditorWidgets::EnumCombo("Shading", render.shadingType);
	EditorWidgets::EnumCombo("Blend", render.blendMode);
	EditorWidgets::EnumCombo("Rasterizer", render.rasterizerType);
	EditorWidgets::EnumCombo("Depth", render.depthMode);
	EditorWidgets::EnumCombo("Billboard", render.billboard);

	// --- 色（0xRRGGBBAAで持っているので、編集のときだけ0〜1のfloatに直す）---
	float rgba[4] = {
	    static_cast<float>((render.color >> 24) & 0xFF) / 255.0f,
	    static_cast<float>((render.color >> 16) & 0xFF) / 255.0f,
	    static_cast<float>((render.color >> 8) & 0xFF) / 255.0f,
	    static_cast<float>(render.color & 0xFF) / 255.0f,
	};
	if (ImGui::ColorEdit4("Color", rgba)) {
		render.color = (static_cast<uint32_t>(rgba[0] * 255.0f + 0.5f) << 24) | (static_cast<uint32_t>(rgba[1] * 255.0f + 0.5f) << 16) | (static_cast<uint32_t>(rgba[2] * 255.0f + 0.5f) << 8) |
		               static_cast<uint32_t>(rgba[3] * 255.0f + 0.5f);
	}

	// --- マテリアルの調整（モデルのmtl / glTFの値を上書きする）---
	if (ImGui::TreeNode("Material")) {
		ImGui::ColorEdit3("Ambient", &render.material.ambient.x);
		ImGui::ColorEdit3("Diffuse", &render.material.diffuse.x);
		ImGui::ColorEdit3("Specular", &render.material.specular.x);
		ImGui::ColorEdit3("Emissive", &render.material.emissive.x);
		ImGui::DragFloat("Shininess", &render.material.shininess, 0.5f, 0.0f, 256.0f);
		ImGui::SliderFloat("Metallic", &render.material.metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &render.material.roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Alpha Cutoff", &render.material.alphaCutoff, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	// --- テクスチャのUVをずらす・回す（Vector3の先頭アドレスを渡してx,yだけ触る）---
	if (ImGui::TreeNode("UV Transform")) {
		ImGui::DragFloat2("Offset", &render.uvTransform.translation.x, 0.01f);
		ImGui::DragFloat2("Tiling", &render.uvTransform.scale.x, 0.01f);
		ImGui::DragFloat("Rotate", &render.uvTransform.rotation.z, 0.01f);
		ImGui::TreePop();
	}
}