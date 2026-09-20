#include "ModelRendererEditor.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include <externals/imgui/imgui.h>

#include "MyEngine/Editor/Widgets/EditorWidgets.h"
#include "MyEngine/Entity/EntityManager.h"
#include "MyEngine/Graphics/Model/ModelManager.h"

namespace {
constexpr const char* kModelSearchRoots[] = {"resources", "MyEngine/Resources"}; // モデルを探すフォルダ（ゲーム側とエンジン側）。無いフォルダは飛ばす
constexpr const char* kModelExtensions[] = {".obj", ".gltf", ".glb", ".fbx"};    // モデルとして扱う拡張子

std::vector<std::string> modelFiles; // 見つかったモデルのパス
bool modelFilesScanned = false;      // 1回でも走査したか

// 拡張子がモデルのものか（大文字でも通るように小文字へ直して比べる）
bool IsModelFile(const std::filesystem::path& path) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	for (const char* candidate : kModelExtensions) {
		if (extension == candidate) {
			return true;
		}
	}
	return false;
}

// フォルダを掘って、モデルファイルのパスを集める。毎フレームやるとディスクを叩き続けるので、1回だけ
void ScanModelFiles() {
	modelFiles.clear();
	for (const char* root : kModelSearchRoots) {
		std::error_code error; // 例外ではなくエラーコードで受ける（フォルダが無くても止まらない）
		if (!std::filesystem::exists(root, error)) {
			continue;
		}
		auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, options, error)) {
			if (entry.is_regular_file(error) && IsModelFile(entry.path())) {
				modelFiles.push_back(entry.path().generic_string()); // 区切りを / に統一する
			}
		}
	}
	std::sort(modelFiles.begin(), modelFiles.end());
	modelFilesScanned = true;
}
} // namespace


//=============================================================================
// Inspectorの中身
//=============================================================================
void ModelRendererEditor::DrawComponent(ModelRendererComponent& render) const {
	ImGui::Checkbox("Enabled", &render.enabled);

	// --- どのモデルを描くか ---
	DrawModelPicker(render);

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

//=============================================================================
// モデルを選ぶコンボ
//=============================================================================
void ModelRendererEditor::DrawModelPicker(ModelRendererComponent& render) const {
	// 一覧は最初に開いたときだけ作る
	if (!modelFilesScanned) {
		ScanModelFiles();
	}

	// 今選ばれているモデルの名前（未選択なら (none)）
	const std::string& currentName = ModelManager::GetModelName(render.modelHandle);
	const char* label = currentName.empty() ? "(none)" : currentName.c_str();

	if (ImGui::BeginCombo("Model", label)) {
		for (const std::string& path : modelFiles) {
			if (ImGui::Selectable(path.c_str())) {
				// 同じパスならキャッシュが返るので、選び直しても読み込み直さない
				render.modelHandle = ModelManager::Load(path);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		ScanModelFiles(); // フォルダにモデルを増やしたとき用
	}
	ImGui::TextDisabled("%zu files / handle=%u", modelFiles.size(), render.modelHandle);
}